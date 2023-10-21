U0 SysBadFree(I64 *ptr)
{
   "AT:%p\n",ptr;
   throw('BadFree');
}

U0 SysBadMAlloc(I64 *ptr)
{
  throw('BadMallo');
}
#ifdef BC_ENABLE
class CBlkCacheEnt {
  U8 *s,*e;
  CMemBlk *mem_blk;
  I64 spin_lock;
};
#define BLK_CACHE_MASK 0xfff
class CBlkCache {
  CBlkCacheEnt ents[BLK_CACHE_MASK+1][16];
  I64 indexes[BLK_CACHE_MASK+1];
  I64 spin_lock;
  CQue all_blocks;
} blk_cache;

I64 HashPtr(U64 ptr) {
  ptr>>=MEM_PAG_BITS;
  return ptr*((ptr>>8)&0xff);
}
CMemBlk *GetMemBlkFromCache(CBlkCache *cache,U8 *ptr) {
  I64 bucket=HashPtr(ptr)&BLK_CACHE_MASK,idx;
  CBlkCacheEnt *ent;
  for(idx=0;idx!=16;idx++) {
    ent=&cache->ents[bucket][idx];
    while(LBts(&ent->spin_lock,0))
      PAUSE;
    if(ent->s<=ptr<ent->e) {
      LBtr(&ent->spin_lock,0);  
      return ent->mem_blk;
    }
    LBtr(&ent->spin_lock,0);  
  }
  return NULL;
}
U0 MemBlkCacheAdd(CBlkCache *cache,CMemBlk *blk) {
  I64 p=blk->pags,bucket,i;
  U8 *ref=blk;
  CBlkCacheEnt *ent;
  while(p--) {
    bucket=HashPtr(blk(U8*)+p<<MEM_PAG_BITS)&BLK_CACHE_MASK;
    while(LBts(&cache->spin_lock,0))
      PAUSE;
    i=cache->indexes[bucket]++;
    cache->indexes[bucket]&=0xf;
    ent=&cache->ents[bucket][i];
    ent->s=blk;
    ent->e=blk(U8*)+blk->pags<<MEM_PAG_BITS;
    ent->mem_blk=blk;
    LBtr(&cache->spin_lock,0);
  }
}
U0 MemBlkCacheRemove(CBlkCache *cache,CMemBlk *blk) {
  I64 p=blk->pags,bucket,i;
  U8 *ref=blk;
  CBlkCacheEnt *ent;
  while(p--) {
    bucket=HashPtr(blk(U8*)+p<<MEM_PAG_BITS)&BLK_CACHE_MASK;
    while(LBts(&cache->spin_lock,0))
      PAUSE;
    for(i=0;i!=16;i++) {
      ent=&cache->ents[bucket][i];
      if(ent->mem_blk==blk) {
        while(LBts(&ent->spin_lock,0))
	  PAUSE;
        ent->s=NULL;
        ent->e=NULL;
        ent->mem_blk=NULL;
        LBtr(&ent->spin_lock,0);
      }
    }
    LBtr(&cache->spin_lock,0);
  }
}
Bool IsValidPtr(U8 *ptr,I64 sz=1) {
  CMemBlk *blk=GetMemBlkFromCache(&blk_cache,ptr),*cur_base;
  CQue *cur;
  cur_base=blk;
  I64 idx,off_by;
  if(!blk) {
    for(cur=blk_cache.all_blocks.next;cur!=&blk_cache.all_blocks;cur=cur->next) {
      cur_base=cur(U8*)-offset(CMemBlk.next2);
      if(cur_base<=ptr<cur_base(U8*)+cur_base->pags<<MEM_PAG_BITS) {
        blk=cur_base;
        MemBlkCacheAdd(&blk_cache,blk);
        break;
      }
    }
  }
  if(blk) {
    if(blk->shadow[idx=(ptr-cur_base)>>3])
       goto found;
    while(idx) {
      if(blk->shadow[idx]) {
        TOSPrint("Access %X is %X bytes OOB\n",ptr,ptr-(idx*8+cur_base(U8*)));
fail:
        TOSPrint("[0]%P\n",Caller(1));
        TOSPrint("[1]%P\n",Caller(2));
        TOSPrint("[2]%P\n",Caller(3));
        TOSPrint("[3]%P\n",Caller(4));
        TOSPrint("[5]%P\n",Caller(5));
        return FALSE;
      }
      --idx;
    }
  }
  if(!__IsValidPtr(ptr)) {
    TOSPrint("Access %X is really out of bounds\n",ptr);
    goto fail;
  }
found:
  return TRUE;
}
#else
Bool IsValidPtr(U8 *ptr,I64 sz=1) {
  "Use '-b' on 3d_loader to enable bounds-checker.\n";
  throw('UseBC');
};
#endif
U8 *MemPagAlloc(I64 pags,CBlkPool *bp=NULL)
{/*Alloc pags from BlkPool. Don't link to task.
(Linking to a task means they will be freed when the task dies.)
It might give you more than you asked for.

Return: NULL if out of memory.
*/
  CMemBlk *res=NULL,*m;
  I64 i;
  Bool old;
  PUSHFD
  old=!Bts(&(Fs->task_flags),TASKf_BREAK_LOCKED); 
  res=NewVirtualChunk(pags<<MEM_PAG_BITS,!bp);
  if(!res) return NULL;
  ins:
  QueInit(res);
  QueInit(&res->next2);
  res->pags=pags;
  res->mb_signature=MBS_UNUSED_SIGNATURE_VAL;
#ifdef BC_ENABLE
  QueIns(&res->next2,blk_cache.all_blocks.last);
  MemBlkCacheAdd(&blk_cache,res);
  res->shadow=NewVirtualChunk(pags<<MEM_PAG_BITS/8,FALSE);
  MemSet(res->shadow,FALSE,pags<<MEM_PAG_BITS/8);
  MemSet(res->shadow,8,sizeof CMemBlk/8);
#endif

at_done:
at_done2:
  if(old)
	BreakUnlock;
  POPFD
  return res;
}

U0 MemPagFree(CMemBlk *m,CBlkPool *bp=NULL)
{//Return non-task pags to BlkPool.
  I64 i,pags;
  Bool old;
  if (m) {
    PUSHFD
    old=!Bts(&(Fs->task_flags),TASKf_BREAK_LOCKED); 
    pags=m->pags;
    m->mb_signature=MBS_UNUSED_SIGNATURE_VAL;
#ifdef BC_ENABLE
    MemBlkCacheRemove(&blk_cache,m);
    FreeVirtualChunk(m->shadow,pags<<MEM_PAG_BITS/8); 
    QueRem(&m->next2);
#endif
    FreeVirtualChunk(m,pags<<MEM_PAG_BITS);
    if(old)
	  BreakUnlock;
    POPFD
  }
}

CMemBlk *MemPagTaskAlloc(I64 pags,CHeapCtrl *hc)
{/*hc must be locked.  Don't preempt this routine.
Currently, this is only called from $LK,"MAlloc",A="MN:MAlloc"$().
Return: NULL if out of memory.
*/
  CMemBlk *res;
  I64 threshold,cnt,size;
  CMemUnused *uum,**_uum,**_ptr;
  if (res=MemPagAlloc(pags,hc->bp)) {
    QueIns(res,hc->last_mem_blk);
    res->mb_signature=MBS_USED_SIGNATURE_VAL;
    hc->alloced_u8s+=res->pags<<MEM_PAG_BITS;

    //Tidy-up free lst (Move into heap hash)
    //because if free lst gets long, delay causes crash.
    threshold=MEM_HEAP_HASH_SIZE>>4;
#assert MEM_HEAP_HASH_SIZE>>4>=sizeof(U8 *)
    do {
      cnt=0;
      _uum=&hc->malloc_free_lst;
      while (uum=*_uum) {
#assert !offset(CMemUnused.next)
	size=uum->size;
	if (size<threshold) {
	  *_uum=uum->next;
	  _ptr=(&hc->heap_hash)(U8 *)+size;
	  uum->next=*_ptr;
	  *_ptr=uum;
	} else {
	  cnt++;
	  _uum=uum;
	}
      }
      threshold<<=1;
    } while (cnt>8 && threshold<=MEM_HEAP_HASH_SIZE);
  }
  return res;
}

U0 MemPagTaskFree(CMemBlk *m,CHeapCtrl *hc)
{//hc must be locked
  Bool old;
  if (m) {
    PUSHFD
    old=!Bts(&(Fs->task_flags),TASKf_BREAK_LOCKED); 
    if (m->mb_signature!=MBS_USED_SIGNATURE_VAL)
      SysBadFree(m);
    else {
      QueRem(m);
      hc->alloced_u8s-=m->pags<<MEM_PAG_BITS;
      MemPagFree(m,NULL);
    }
    if(old)
	  BreakUnlock;
    POPFD
  }
}

asm {
	_SYS_WHINE_OOB::
	PUSH	RBP
	MOV	RBP,RSP
 	PUSH_C_REGS
	PUSH	U64 SF_ARG2[RBP]
	PUSH	U64 SF_ARG1[RBP]
	CALL	&IsValidPtr
	POP_C_REGS
	LEAVE
	RET1	16
};
U0 InitHeaps() {
#ifdef BC_ENABLE
  MemSet(&blk_cache,0,sizeof CBlkCache);
  QueInit(&blk_cache.all_blocks);
#endif
}