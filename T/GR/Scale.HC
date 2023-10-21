/**
 * Heres the deal,this is code written by nroot for scaling the screen to higher resolutions
 * There are 2 mixing tables,one takes 2 pixels and the other takes 4 pixels
 * The 2 pixel table goes across the x-axis and is used for "inbetween" pixels 
 *   [x+0]<--mixed-->[x+1]
 * The 4 pixel table will across the y-axis,but takes account for the x-axises mixed pixels
 *  [x+0]<--mixef-->[x+1] //Y+0
 *  /\     /\        /\
 *  ||     ||        ||
 *  \/     \/        \/
 *  [x+0]<--mixed-->[x+1] //Y+1
 *  [x+0] is mixed  with ([x,y],[x,y],[x,y+1],[x,y+1])
 *  The Inbetween is mixed  with ([x,y],[x+1,y],[x,y+1],[x+1,y+1])
 *
*
 * The internal resolution is still 640/480,but we use the grx/gry_offs tables
 * to map the cordnates to the scaled screen. The inbetween pixels are mixed (see above) 
 * 
 * In window.c and WIN_window.c, the ScalerMP is called in multiple threads. It's pretty epic
 */
U32 pal[16]={
(0x000000),(0x0000AA),(0x000AA00),(0x00AAAA),
(0xAA0000),(0xAA00AA),(0xAA5500),(0xAAAAAA),
(0x555555),(0x5555FF),(0x55FF55),(0x55ffff),
(0xFF5555),(0xFF55FF),(0xFFFF55),(0xFFFFFF)
};
//We store the lower 4 bits in byte 1,upper 4 bits at byte 2 
U32 *pal_mixed,*pal_mixed2;
U0 AvTabGen() {
  /*
  16 color
  0x0-0xf

  //the first  byte is the first row
  //the second is the next rows colors

  1      2
  [f][=][a]
   [b][=][c]

   0xfabc
       U32 RGB=gr_scale_pal2[0xfabc]
       REPEAT STORE
   use the first pixel
   repeat the fill for the gap

  */
  if(!pal_mixed) pal_mixed=CAlloc(0x1000*4,adam_task);
  if(!pal_mixed2) pal_mixed2=CAlloc(0x10000*4,adam_task);
  I64 a,b;
  U32 c,c2;
  U32 mixed,mixed2;
  for(a=0;a!=0x100;a++) {
     b=(a&0xf0)>>4;
     c=pal[a&0xf],c2=pal[a>>4];
     //https://www.compuphase.com/graphic/scale3.htm
     pal_mixed[(a&0xf)|b<<8]=((c^c2)&0xfffefefe)>>1+(c&c2);
  }
  for(b=0;b!=0x100;b++)
    for(a=0;a!=0x100;a++) {
       c=pal[a&0xf],c2=pal[a>>4];
       //https://www.compuphase.com/graphic/scale3.htm
       mixed=((c^c2)&0xfffefefe)>>1+(c&c2);
       c=pal[b&0xf],c2=pal[b>>4];
       mixed2=((c^c2)&0xfffefefe)>>1+(c&c2);
       c=mixed,c2=mixed2;
       pal_mixed2[a|b<<8]=((c^c2)&0xfffefefe)>>1+(c&c2);
       
    }
}
AvTabGen;
U0 GrPaletteColorSet(I64 i /*The color index*/,CBGR48 v /*this is the rgb color*/){
  U32 c=0;
  if(!__3DaysSwapRGB) {
    c.u8[0]=v.b/ToF64(0xffff)*0xff;
    c.u8[1]=v.g/ToF64(0xffff)*0xff;
    c.u8[2]=v.r/ToF64(0xffff)*0xff;
  } else {
    c.u8[2]=v.b/ToF64(0xffff)*0xff;
    c.u8[1]=v.g/ToF64(0xffff)*0xff;
    c.u8[0]=v.r/ToF64(0xffff)*0xff;
  }
  __3DaysSetVGAColor(i,c);
  pal[i]=c;
  AvTabGen;
}
U0 GrPaletteColorGet(I64 i) {
  CBGR48 r;
  U32 c=pal[i];
  if(!__3DaysSwapRGB) {
    r.b=c.u8[0]*(ToF64(0xffff)/0xff);
    r.g=c.u8[1]*(ToF64(0xffff)/0xff);
    r.r=c.u8[2]*(ToF64(0xffff)/0xff);
  } else {
    r.b=c.u8[2]*(ToF64(0xffff)/0xff);
    r.g=c.u8[1]*(ToF64(0xffff)/0xff);
    r.r=c.u8[0]*(ToF64(0xffff)/0xff);
  }
  return r;
}
U32 *grx_offs=CAlloc(641*4,adam_task);
U32 *gry_offs=CAlloc(481*4,adam_task);
//64 cores maxmimum?
I64 *scale_mp_starts=NULL;
U0 UpdateGROffs(I64 x,I64 y) {
  static I64 x2,y2;
  F64 idx;
  I64 cnt;
  F64 yspacing=ToF64(y)/480;
  F64 xspacing=ToF64(x)/640;
  if(x!=x2||y2!=y) {
    if(!scale_mp_starts) scale_mp_starts=CAlloc((mp_cnt+1)*sizeof(I64),adam_task);
    for(idx=0;idx!=mp_cnt;idx++)
      scale_mp_starts[idx]=idx*(GR_HEIGHT/mp_cnt);
    scale_mp_starts[idx]=GR_HEIGHT;
    x2=x,y2=y;
    for(cnt=0;cnt<641;cnt++)
      grx_offs[cnt]=Round(xspacing*cnt);
    for(cnt=0;cnt<481;cnt++)
      gry_offs[cnt]=Round(yspacing*cnt);
    //Remove "big" gaps
    for(cnt=1;cnt<640;cnt++)
      grx_offs[cnt]=(ToF64(grx_offs[cnt])+ToF64(grx_offs[cnt+1]))/2.;   
    for(cnt=0;cnt<480;cnt++)
      gry_offs[cnt]=(ToF64(gry_offs[cnt])+ToF64(gry_offs[cnt+1]))/2.;   
  }
}
U0 Scale(CDC *data,I64 sx,I64 sy,CDC *result,I64 xoff,I64 yoff,I64 core) {
  I64 noreg y_next_px;
asm {
  IMPORT scale_mp_first_row_completed;
  IMPORT grx_offs;
  IMPORT gry_offs;
  IMPORT pal_mixed;
  IMPORT pal_mixed2;
  IMPORT pal;
  IMPORT scale_mp_starts;
  PUSH_REGS
  PUSH RAX
  FSTCW U16[RSP]
  MOV RAX,U64 [RSP]
  //Set round-to-nearest mode
  OR RAX,0xc00
  PUSH RAX
  FLDCW U16[RSP]
  POP RAX
  /*
   R8 is av_Tab_gen
   R9 is result
   R10 is data
   R11 is x
   R12 is y
   R13 is data.body+off
   R15 is result body pointer(translated to [xoff,yoff])
   RSI is data pointer
   RDI is result pointer
  */
  MOV R13,SF_ARG2[RBP]
  MOV R14,SF_ARG3[RBP]
  MOV R9,SF_ARG4[RBP]
  MOV R10,SF_ARG1[RBP]
  
  MOV R15,U64 SF_ARG6[RBP]
  MOV ECX,U32 CDC.width_internal[R9]
  IMUL2 R15,RCX
  ADD R15,U64 SF_ARG5[RBP]
  LEA R15,U64 [R15*4]
  ADD R15,U64 CDC.body[R9]
  MOV RDI,R15
//y=scale_mp_stars[Gs->num]
  MOV RAX,U64 &core[RBP]
  MOV RBX,U64 [scale_mp_starts]
  MOV R12,U64 [RBX+RAX*8]
  yloop:
//data_ptr=data->body+data->width_internal*(y/yper_px1)
  MOV ECX,U32 CDC.width_internal[R10]
  IMUL2 RCX,R12
  MOV RSI,U64 CDC.body[R10]
  ADD RSI,RCX
  MOV R13,RSI
  
//dst_ptr=result->data+result->width_internal*y
  MOV RDI,U64 [gry_offs]
  MOV EBX,U32 4[RDI+R12*4]
  MOV I64 &y_next_px[RBP],RBX
  MOV EDI,U32 [RDI+R12*4]
  
  MOV ECX,U32 CDC.width_internal[R9]
  IMUL2 RDI,RCX
  LEA RDI,U64 [RDI*4]
  ADD RDI,R15
  
  //x=0
  MOV R11,0
xloop:
  MOVZX EAX,U8[RSI+R11]
  AND AL,0xf;
  MOV EAX,U32 pal[RAX*4]
  MOV R14,U64 [grx_offs]
  MOV EBX,U32 4[R14+R11*4] //next pixel
  MOV R14u32,U32 [R14+R11*4]
  MOV U32 [RDI+R14*4],EAX
  CMP RBX,R14
  JE I32 next
  //Dont go past next pixel
  CMP R11,639
  JE I32 next
  MOV RCX,RBX
  SUB RCX,R14
  //Avg between 2 color bytes
  MOVZX EAX,U16 [RSI+R11]
  //Only want first 4 bits of each byte
  AND EAX,0xf0f 
  MOV RBX,U64 [pal_mixed]
  MOV EAX,U32 [RBX+RAX*4]
  PUSH RDI
  LEA RDI,U64 4[RDI+R14*4]
  REP_STOSD
  POP RDI
next:
  INC R11
  CMP R11,640
  JNE U32 xloop
  
  MOV RDI,U64 [gry_offs]
  MOV EDI,U32 [RDI+R12*4]
  MOV RCX,I64 &y_next_px[RBP] 
next2:
////Get start of next core's start line,loop until then
  MOV RAX,U64 &core[RBP]
  MOV RBX,U64 [scale_mp_starts]
  CMP R12,U64[RBX+RAX*8] //First row
  JNE skip_bt
  LOCK BTS U64 [scale_mp_first_row_completed],RAX
skip_bt:
  INC R12
  INC RAX
  CMP R12,U64 [RBX+RAX*8]
  JNE U32 yloop
//
  MOV R14,U64 CDC.body[R10]
////Get start of next core's start line,loop until then
  MOV RAX,U64 &core[RBP]
  MOV RBX,U64 [scale_mp_starts]
  MOV R12,U64 [RBX+RAX*8]
  INC RAX
  MOV R13,U64 [RBX+RAX*8]
  //Make R14 point to current scan line(R12)
  MOV EBX,U32 CDC.width_internal[R10]
  IMUL2 RBX,R12
  ADD R14,RBX
yloop2:
  MOV RDI,U64 [gry_offs]
  MOV EBX,U32 4[RDI+R12*4]
  MOV EDI,U32 [RDI+R12*4]
  DEC RBX
  CMP RBX,RDI
  JNG I32 next3
//
// Because this routine is called parallel,we must wait for the first row of the
// next core's row to be complete when we merge at the last line(if not a 480) 
//
  CMP R12,479
  JE I32 skip_first_row_check
  LEA RAX,U64 1[R12]
  CMP RAX,R13
  JNE I32 skip_first_row_check
  MOV RAX,U64 &core[RBP]
  INC RAX
wait_next_core:
  BT U64[scale_mp_first_row_completed],RAX
  JNC I32 wait_next_core
  LOCK BTR U64[scale_mp_first_row_completed],RAX
skip_first_row_check:
//we will repeat until the next line
  MOV RCX,RBX
  SUB RCX,RDI
//get scanline start
  MOV R10u32,U32 CDC.width_internal[R9]
  IMUL2 RDI,R10
  LEA RDI,U64 [RDI*4]
  ADD RDI,R15
  LEA RSI,U64 [RDI+R10*4]
  //push repeat_cnt
  PUSH RCX
  PUSH U64 SF_ARG2[RBP] //sx
  //Point to next line
  PUSH RSI
  PUSH R14
  ADD U64[RSP],640
  PUSH R14
  PUSH R8
  CALL AVG_2_LINES
  MOV RDI,RSI
  MOV RBX,RSI //RBX(from RSI) has source of merged stuff(no need to merge agian,just copy what we have)
//Pop the repeat count
  POP RCX
  DEC RCX
  CMP RCX,0
  JNG I32 next3
  MOV RDI,RSI
  LEA RDI,[RDI+R10*4] //r10 has scanline width
row_loop:
  PUSH RCX
  //RBX has start of already computed row
  MOV RSI,RBX
  PUSH RDI //Store RDI at start of scanline
  //RDI has destination
  MOV RCX,SF_ARG2[RBP]
  LEA RCX,U64[RCX*4]
  REP_MOVSD
  POP RDI //pop to get start of scanline
  LEA RDI,U64 [RDI+R10*4] //r10 has scanline width
  POP RCX
  LOOP row_loop
next3:
  ADD R14,640
  INC R12
  CMP R12,R13
  JNE U32 yloop2
  FLDCW U16[RSP]
  POP RAX
  POP_REGS
  LEAVE
  RET1 7*8
//Avg2Lines(U8 *avg_tab,u8 *line1,U8 *line2,u32 *to,U64 sx)
AVG_2_LINES:
    PUSH RBP
    MOV RBP,RSP
    PUSH R8
    PUSH R9
    PUSH R10
    PUSH RDI
    PUSH R12
    PUSH R13
    PUSH R14
    MOV R9,U64 SF_ARG2[RBP]
    MOV R10,U64 SF_ARG3[RBP]
    MOV RDI,U64 SF_ARG4[RBP]
    MOV R8,0
    MOV R14,U64 [grx_offs]
    MOV RBX,U64 [pal_mixed]
@@loop:
    MOV R12u32,U32 4[R14+R8*4] //NExt scaled x pixel
    MOV R13u32,U32 [R14+R8*4]
   
    //Mix orthodox pixels
    //Form 16bit color color code to avergage
    MOV CL,U8[R9+R8]
    MOV AL,U8[R10+R8] //REX promotes AH to SPL
    MOV AH,CL //See above note
    MOVZX EAX,AX
    //Only want first 4 bits of each byte
    AND EAX,0xf0f 
    MOV EAX,U32 [RBX+EAX*4]
    STOSD
    //
    MOV RCX,R12
    SUB RCX,R13
    DEC RCX
    CMPRCX,0
    JNG @@skip
    MOV DX,U16[R9+R8]
    AND EDX,0xf0f
    MOV AX,U16[R10+R8] //REX promotes AH to SPL
    AND EAX,0xf0f
    MOVZX EAX,AX
    SHL EAX,4
    OR EAX,EDX
    MOV RDX,U64 [pal_mixed2]
    MOV EAX,U32 [RDX+RAX*4]
    REP_STOSD
    //
@@skip:
    INC R8
    CMP R8,640
    JNE @@loop
    POP R14
    POP R13
    POP R12
    POP RDI
    POP R10
    POP R9
    POP R8
    LEAVE 
    RET1 5*8
   };
}
I64 scale_mp_first_row_completed=0;
class CScalerThreadArgs {
  CDC *data,*scrn;
  I64 sx,sy,xoff,yoff;
} scale_mp_args;
//Called from window.c/WIN_window.c from ScaleScrn(int64_t core)
U0 ScalerMP(I64 idx) {
  Scale(
    scale_mp_args.data,
    scale_mp_args.sx,
    scale_mp_args.sy,
    scale_mp_args.scrn,
    scale_mp_args.xoff,
    scale_mp_args.yoff,
    idx
    );
}
