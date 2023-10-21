#help_index "Graphics/Scrn"

U0 GrUpdateTaskODEs(CTask *task)
{
  sys_task_being_scrn_updated=task;
  try
    ODEsUpdate(task);
  catch {
    LBts(&task->win_inhibit,WIf_SELF_ODE);
    "Exception in WinMgr: Update Task ODEs\n";
    PutExcept;
    Sleep(3000);
  }
  sys_task_being_scrn_updated=NULL;
}

U0 GrUpdateTaskWin(CTask *task)
{ //Draw a win.  Only Core0 tasks have a win.
  CDC *dc;
  CD3I64 saved_scroll;
  sys_task_being_scrn_updated=task;
  try {
    if (!Bt(&task->display_flags,DISPLAYf_NO_BORDER))
      TextBorder(Fs,task->win_left,task->win_right,task->win_top,
	    task->win_bottom,task->border_attr,task==sys_focus_task);
    TextRect(task->win_left,task->win_right,
	  task->win_top,task->win_bottom,task->text_attr<<8);
    if (task==sys_winmgr_task) {
      if (gr.fp_wall_paper)
	(*gr.fp_wall_paper)(task);
    } else if (!(task->win_inhibit&WIF_SELF_DOC))
      DocUpdateTaskDocs(task);
    if (TaskValidate(task)) {
      if (task->draw_it) {
	dc=DCAlias(gr.dc2,task);
    (*task->draw_it)(task,dc);
	DCDel(dc);
      }
      if (TaskValidate(task)) {
	WinScrollNull(task,&saved_scroll);
	DrawCtrls(task);
	WinScrollRestore(task,&saved_scroll);
      }
    }
  } catch {
    if (task!=Fs && TaskValidate(task)) {
      LBtr(&task->display_flags,DISPLAYf_SHOW);
      "Exception in WinMgr: Update Task Win\n";
      PutExcept;
      Sleep(3000);
    }
  }
  sys_task_being_scrn_updated=NULL;
}

U0 GrUpdateTasks()
{//Only called by WinMgr
  I64 i;
  CTask *task,*task1;
  try {
    winmgr.ode_time=0;
    WinZBufUpdate;
    task1=task=sys_winmgr_task;
    do { //Loop through Core0 tasks.
      if (!TaskValidate(task)) break;
      if (Bt(&task->display_flags,DISPLAYf_SHOW) &&
	    Bt(gr.win_uncovered_bitmap,task->win_z_num))
	GrUpdateTaskWin(task);
      if (!TaskValidate(task)) break;
      task=task->next_task;
    } while (task!=task1);

    for (i=0;i<mp_cnt;i++) { //Loop through all cores.
      task1=task=cpu_structs[i]->seth_task;
      do {
	if (!TaskValidate(task)) break;
	GrUpdateTaskODEs(task);
	if (!TaskValidate(task)) break;
	task=task->next_task;
      } while (task!=task1);
    }
  } catch {
    PutExcept(FALSE);
    //Dbg("Exception in WinMgr"); TODO RESTORE
  }
  winmgr.last_ode_time=winmgr.ode_time;
  ode_alloced_factor=LowPass1(0.1,ode_alloced_factor,
	Clamp(Gs->idle_factor-0.1,0.2,0.8),1/winmgr.fps);
  sys_task_being_scrn_updated=NULL;
}

U0 GrFixZoomScale()
{
  gr.scrn_zoom=ClampI64(gr.scrn_zoom,1,GR_SCRN_ZOOM_MAX);
  if (gr.scrn_zoom==1) {
    gr.sx=0;
    gr.sy=0;
  } else {
    gr.sx=ClampI64(gr.sx,0,GR_WIDTH-GR_WIDTH/gr.scrn_zoom)&~7;
    gr.sy=ClampI64(gr.sy,0,GR_HEIGHT-GR_HEIGHT/gr.scrn_zoom);
  }
}

public U0 GrScaleZoom(F64 scale)
{//Multiply zoom factor larger or smaller.
  F64 s=gr.scrn_zoom;
  gr.scrn_zoom=gr.scrn_zoom*scale;
  GrFixZoomScale;
  s/=gr.scrn_zoom;
  ms.scale.x*=s;
  ms.scale.y*=s;
  ms.scale.z*=s;
  gr.sx=ms.pos.x;
  gr.sy=ms.pos.y;
  ms.offset.z=ms.pos.z-(ms.pos.z-ms.offset.z)*s;
  GrFixZoomScale;
  ms.offset.x=gr.sx;
  ms.offset.y=gr.sy;
}

U0 GrZoomInScrn()
{
  GrFixZoomScale;
  I64 k,l,span,j,y;
  U8 *src,*src_cap,*dst,*ndst;
  dst=gr.zoomed_dc->body;
  span=gr.zoomed_dc->width/gr.scrn_zoom;
  for(y=0;y!=gr.zoomed_dc->height/gr.scrn_zoom;y++) {
    src=gr.dc2->body+gr.sx+(gr.sy+y)*gr.dc2->width_internal;  
    src_cap=src+span;
//We fill 1 row then we multiply that row,gr.scrn_zoom times
    //This will make our "pixels" sqaure
    while(src!=src_cap) {
      for(k=0;k!=gr.scrn_zoom;k++)
        *dst++=*src;
      src++;
    }
    dst-=span*gr.scrn_zoom;
    //Repeat the zoomed rows as they are the same
    for(l=1;l<gr.scrn_zoom;l++) {
      ndst=dst+gr.zoomed_dc->width_internal;
      MemCpy(ndst,dst,gr.zoomed_dc->width_internal);
      dst=ndst;
    }
    dst=ndst+gr.zoomed_dc->width_internal;
  }
}

U0 GrUpdateTextBG()
{
  I64 reg RSI *dst=gr.dc2->body,reg R13 c,row,col,
	num_rows=TEXT_ROWS,num_cols=TEXT_COLS,i,j,cur_ch,
	reg R12 w1=gr.dc2->width_internal,w2=-7*w1+8,w3=7*w1,w4=0;
  U32 *src=gr.text_base;
  Bool blink_flag=Blink;
  U8 *dst2=dst;

  if (gr.pan_text_x||gr.hide_col) {
    gr.pan_text_x=ClampI64(gr.pan_text_x,-7,7);
    j=AbsI64(gr.pan_text_x)/FONT_WIDTH+1;
    num_cols-=j;
    if (gr.pan_text_x<0) {
      src+=j;
      i=FONT_WIDTH*j+gr.pan_text_x;
    } else
      i=gr.pan_text_x;
    dst2=dst(U8 *)+i;
    w4=j;
    w3+=j*FONT_WIDTH;

    j*=FONT_WIDTH;
    dst(U8 *)=gr.dc2->body;
    for (row=num_rows*FONT_HEIGHT;row--;) {
      for (col=i;col--;)
	*dst(U8 *)++=0;
      dst(U8 *)+=w1-i-j;
      for (col=j;col--;)
	*dst(U8 *)++=0;
    }
  }
  dst=dst2;

  if (gr.pan_text_y||gr.hide_row) {
    gr.pan_text_y=ClampI64(gr.pan_text_y,-7,7);
    j=AbsI64(gr.pan_text_y)/FONT_HEIGHT+1;
    num_rows-=j;
    if (gr.pan_text_y<0) {
      src+=w1/FONT_WIDTH*j;
      i=w1*(FONT_HEIGHT*j+gr.pan_text_y);
    } else
      i=w1*gr.pan_text_y;
    dst2=dst(U8 *)+i;

    j*=w1*FONT_HEIGHT;
    dst(U8 *)=gr.dc2->body;
    for (row=i;row--;)
      *dst(U8 *)++=0;
    dst(U8 *)=gr.dc2->body+TEXT_ROWS*TEXT_COLS*FONT_HEIGHT*FONT_WIDTH-j;
    for (row=j;row--;)
      *dst(U8 *)++=0;
  }
  dst=dst2;

  for (row=num_rows;row--;) {
    for (col=num_cols;col--;) {
      cur_ch=*src++;
      if (cur_ch & (ATTRF_SEL|ATTRF_INVERT|ATTRF_BLINK)) {
	if (cur_ch & ATTRF_SEL)
	  cur_ch.u8[1]=cur_ch.u8[1]^0xFF;
	if (cur_ch & ATTRF_INVERT)
	  cur_ch.u8[1]=cur_ch.u8[1]<<4+cur_ch.u8[1]>>4;
	if (cur_ch & ATTRF_BLINK && blink_flag)
	  cur_ch.u8[1]=cur_ch.u8[1]<<4+cur_ch.u8[1]>>4;
      }
      c=gr.to_8_colors[cur_ch.u8[1]>>4];
      MOV	U64 [RSI],R13
      ADD	RSI,R12
      MOV	U64 [RSI],R13
      ADD	RSI,R12
      MOV	U64 [RSI],R13
      ADD	RSI,R12
      MOV	U64 [RSI],R13
      ADD	RSI,R12
      MOV	U64 [RSI],R13
      ADD	RSI,R12
      MOV	U64 [RSI],R13
      ADD	RSI,R12
      MOV	U64 [RSI],R13
      ADD	RSI,R12
      MOV	U64 [RSI],R13
      dst(U8 *)+=w2;
    }
    src+=w4;
    dst(U8 *)+=w3;
  }
}

U0 GrUpdateTextFG()
{//See \dLK,"TextBase Layer",A="HI:TextBase Layer"\d.
  U32 *src=gr.text_base;
  I64 i,j,cur_ch,*dst=gr.dc2->body,
	w1=gr.dc2->width_internal,w2=7*w1,w4=0,
	num_rows=TEXT_ROWS,num_cols=TEXT_COLS,row,col;
  U8 *dst_start=gr.dc2->body,*dst_end=dst_start+w1*gr.dc2->height-7*w1-8;
  Bool blink_flag=Blink;

  if (gr.pan_text_x||gr.hide_col) {
    gr.pan_text_x=ClampI64(gr.pan_text_x,-7,7);
    j=AbsI64(gr.pan_text_x)/FONT_WIDTH+1;
    num_cols-=j;
    if (gr.pan_text_x<0) {
      src+=j;
      dst(U8 *)+=FONT_WIDTH*j;
    }
    w4=j;
    w2+=j*FONT_WIDTH;
  }

  if (gr.pan_text_y||gr.hide_row) {
    gr.pan_text_y=ClampI64(gr.pan_text_y,-7,7);
    j=AbsI64(gr.pan_text_y)/FONT_HEIGHT+1;
    num_rows-=j;
    if (gr.pan_text_y<0) {
      src+=w1/FONT_WIDTH*j;
      dst(U8 *)+=w1*FONT_HEIGHT*j;
    }
  }

  for (row=num_rows;row--;) {
    for (col=num_cols;col--;) {
      cur_ch=*src++;
      if (cur_ch & (ATTRF_UNDERLINE|ATTRF_SEL|ATTRF_INVERT|ATTRF_BLINK)) {
	if (cur_ch & ATTRF_SEL)
	  cur_ch.u8[1]=cur_ch.u8[1]^0xFF;
	if (cur_ch & ATTRF_INVERT)
	  cur_ch.u8[1]=cur_ch.u8[1]<<4+cur_ch.u8[1]>>4;
	if (cur_ch & ATTRF_BLINK && blink_flag)
	  cur_ch.u8[1]=cur_ch.u8[1]<<4+cur_ch.u8[1]>>4;
      }
      if (i=cur_ch.u16[1]&0x3FF+gr.pan_text_x+gr.pan_text_y<<5) {
	j=i&0x1F;
	if (j&0x10) j|=~0x1F;
	i>>=5;
	if (i&0x10) i|=~0x1F;
	i=w1*i+j;
	if (dst_start<=dst(U8 *)+i<dst_end)
	  GrRopEquU8NoClipping(cur_ch&(ATTRF_UNDERLINE+0xFFF),dst(U8 *)+i,w1);
      } else
	GrRopEquU8NoClipping(cur_ch&(ATTRF_UNDERLINE+0xFFF),dst,w1);
      dst(U8 *)+=8;
    }
    src+=w4;
    dst(U8 *)+=w2;
  }
}

U0 DCBlotColor8(CDC *dc,CDC *img)
{
  U8  *src=img->body,*b0=dc->body;
  I64 j,k,d0=img->width_internal*img->height;
  for (k=0;k<d0;k++) {
    j=*src++;
    if (j!=TRANSPARENT)
      *b0++=j;
    else
      b0++;
  }
}

U0 GrUpdateTextModeText()
{
  U32 *src=gr.text_base;
  I64 cur_ch,i=TEXT_COLS*TEXT_ROWS;
  U16 *dst=text.vga_text_alias,*dst2=gr.vga_text_cache;
  Bool blink_flag=Blink;
  if (LBtr(&sys_semas[SEMA_FLUSH_VGA_IMAGE],0)) {
    while (i--) {
      cur_ch=*src++;
      if (cur_ch & ATTRF_SEL)
	cur_ch.u8[1]=cur_ch.u8[1]^0xFF;
      if (cur_ch & ATTRF_INVERT)
	cur_ch.u8[1]=cur_ch.u8[1]<<4+cur_ch.u8[1]>>4;
      if (cur_ch & ATTRF_BLINK)
	if (blink_flag)
	  cur_ch.u8[1]=cur_ch.u8[1]<<4+cur_ch.u8[1]>>4;
      *dst++=*dst2++=cur_ch&0x7FFF;
    }
  } else {
    while (i--) {
      cur_ch=*src++;
      if (cur_ch & ATTRF_SEL)
	cur_ch.u8[1]=cur_ch.u8[1]^0xFF;
      if (cur_ch & ATTRF_INVERT)
	cur_ch.u8[1]=cur_ch.u8[1]<<4+cur_ch.u8[1]>>4;
      if (cur_ch & ATTRF_BLINK)
	if (blink_flag)
	  cur_ch.u8[1]=cur_ch.u8[1]<<4+cur_ch.u8[1]>>4;
      cur_ch&=0x7FFF;
      if (*dst2!=cur_ch)
	*dst++=*dst2++=cur_ch;
      else {
	dst++;
	dst2++;
      }
    }
  }
}

U0 GrUpdateVGAGraphics()
{//Update Graphic Card
}
U8 *_3days_screen=NULL;
if(!__IsCmdLine)
  _3days_screen=DrawWindowNew;
asm {
  IMPORT pal;
  IMPORT scale_scrn;
  IMPORT scale_scrn_xoff;
  IMPORT scale_scrn_yoff;
  _NO_SCALE::
  PUSH RBP
  MOV RBP,RSP
  PUSH RDI
  PUSH RSI
  PUSH R15
  MOV RDI,U64 [scale_scrn]
  MOV R15u32,U32 CDC.width_internal [RDI]
  MOV RBX,R15 //save for later
  IMUL2 R15,U64 [scale_scrn_yoff]
  ADD R15,U64 [scale_scrn_xoff]
  LEA R15,U64[R15*4]
  LEA RBX,U64[RBX*4]
  
  MOV RSI,U64 SF_ARG1[RBP]
  MOV RSI,U64 CDC.body[RSI]
  MOV RDI,U64 CDC.body[RDI]
  ADD RDI,R15
  
  MOV RCX,GR_HEIGHT
@@loopy:
  MOV RDX,RCX
  MOV RCX,GR_WIDTH  
@@loopx:
  LODSB
  AND RAX,0xf
  MOV EAX,U32 pal[RAX*4]
  STOSD
  LOOP @@loopx
  MOV RCX,RDX
  SUB RDI,GR_WIDTH*4
  ADD RDI,RBX //has internal_width*4
  LOOP @@loopy
  POP R15
  POP RSI
  POP RDI
  LEAVE
  RET1 1*8
};
I64 screen_w,screen_h;
I64 scale_scrn_xoff=0;
I64 scale_scrn_yoff=0;
I64 scale_w;
I64 scale_h;
I64 scale_enable=1;
I64 scale_lock=0;
#include "Scale.HC";
CDC *scale_scrn=NULL;
CTask *scale_lock_task=NULL;
U0 SetScaleResolution(I64 w,I64 h) {
  while(LBts(&scale_lock,0)) {
    if(scale_lock_task==Fs)
      break;
    Yield;
  }
  scale_lock_task=Fs;
  if(!scale_scrn) {
    scale_scrn=DCNew(GR_WIDTH,GR_HEIGHT,adam_task);
    Free(scale_scrn->body);
  }
  w=MaxI64(w,GR_WIDTH);
  h=MaxI64(h,GR_HEIGHT);
  
  scale_scrn->height=h;
  scale_scrn->width=scale_scrn->width_internal=w;
  scale_scrn->body=_3DaysSetResolution(screen_w=w,screen_h=h);
  if(scale_enable) {
    w=MaxI64(GR_WIDTH,w);
    h=MaxI64(GR_HEIGHT,h);
    scale_w=ToF64(h)/480.*640.;
    scale_h=h;
    scale_h=MinI64(h,scale_h);
    if(scale_w>w) {
      //If we cant scale across,decrease height
      scale_h=ToF64(w)/640.*480;
      scale_w=MinI64(w,scale_w);
    }
    scale_scrn_yoff=(h-scale_h)/2;
    scale_scrn_xoff=(w-scale_w)/2;
    //MemSetU32(scale_scrn->body,0,w*h);
  } else {
    MemSet(scale_scrn->body,BLACK,scale_scrn->height*scale_scrn->width*4);
    scale_scrn_yoff=(scale_scrn->height-GR_HEIGHT)/2;
    scale_scrn_xoff=(scale_scrn->width-GR_WIDTH)/2;
    scale_h=GR_HEIGHT;
    scale_w=GR_WIDTH;
  }
  UpdateGROffs(scale_w,scale_h);
  scale_lock_task=NULL;
  LBtr(&scale_lock,0);
}
U0 ScaleMouseCords(I64 *x,I64 *y) {
  I64 x2=*x,y2=*y;
  x2-=scale_scrn_xoff;
  y2-=scale_scrn_yoff;
  x2=(ToF64(x2)/scale_w*640.);
  y2=(ToF64(y2)/scale_h*480.);
  x2=ClampI64(x2,0,640);
  y2=ClampI64(y2,0,480);
  *x=x2;
  *y=y2;
}
_extern _NO_SCALE U0 NoScale(CDC *src);
U0 GrUpdateScrn()
{//Called by the Window Manager \dLK,"HERE",A="FF:::/Adam/WinMgr.HC,GrUpdateScrn"\d, 30 times a second.
  I64 idx;
  CDC *dc;
    GrUpdateTextBG;
    GrUpdateTextFG;
    DCBlotColor8(gr.dc2,gr.dc);
    GrUpdateTasks;
    
  dc=DCAlias(gr.dc2,Fs);
  dc->flags|=DCF_ON_TOP;
  if (gr.fp_final_scrn_update)
    (*gr.fp_final_scrn_update)(dc);
  DCDel(dc);
  if(gr.scrn_zoom==1) {
    dc=gr.dc2;
  } else {
    GrZoomInScrn;
    dc=gr.zoomed_dc;
  }
  if(!scale_scrn)
     SetScaleResolution(640,480);
  while(LBts(&scale_lock,0)) {
    if(scale_lock_task==Fs)
      break;
    Yield;
  }
  scale_lock_task=Fs;
  if(__3DaysVGAMode) {
    DrawWindowUpdate(scale_scrn,dc->body,dc->width_internal,dc->height);  
  } if(scale_w==GR_WIDTH&&scale_h==GR_HEIGHT||!scale_enable) {
    NoScale(dc);
  } else {
    //Scale(dc,scale_w,scale_h,scale_scrn,scale_scrn_xoff,scale_scrn_yoff)
    scale_mp_args.data=dc;
    scale_mp_args.sx=scale_w;
    scale_mp_args.sy=scale_h;
    scale_mp_args.scrn=scale_scrn;
    scale_mp_args.xoff=scale_scrn_xoff;
    scale_mp_args.yoff=scale_scrn_yoff;
    _3DaysScaleScrn();
  }
  DrawWindowUpdate(scale_scrn,scale_scrn->body,scale_scrn->width_internal,scale_scrn->height);
  scale_lock_task=NULL;
  LBtr(&scale_lock,0);
  //God willing they are the same size
  MemCpy(gr.scrn_image->body,gr.dc2->body,MSize(gr.scrn_image->body));
}
U0 EnableScaling(Bool on=FALSE) {
  I64 w,h;
  __3DaysEnableScaling(on);
  if(!on) {
    while(LBts(&scale_lock,0))
      Yield;
    MemSet(scale_scrn->body,BLACK,scale_scrn->height*scale_scrn->width*4);
    scale_scrn_yoff=(scale_scrn->height-GR_HEIGHT)/2;
    scale_scrn_xoff=(scale_scrn->width-GR_WIDTH)/2;
    scale_h=GR_HEIGHT;
    scale_w=GR_WIDTH;
    LBtr(&scale_lock,0);
  } else {
    while(LBts(&scale_lock,0))
      Yield;
    w=screen_w;
    h=screen_h;
    w=MaxI64(GR_WIDTH,w);
    h=MaxI64(GR_HEIGHT,h);
    scale_w=ToF64(h)/480.*640.;
    scale_h=h;
    scale_h=MinI64(h,scale_h);
    if(scale_w>w) {
//If we cant scale across,decrease height
      scale_h=ToF64(w)/640.*480;
      scale_w=MinI64(w,scale_w);
    }
    scale_scrn_yoff=(h-scale_h)/2;
    scale_scrn_xoff=(w-scale_w)/2;
    LBtr(&scale_lock,0);    
  }
  return LBEqu(&scale_enable,0,on);
}
