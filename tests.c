#include "3d.h"
#include <stdio.h>
static void AppendToLexer(char *txt) {
    mrope_append_text(Lexer.source, strdup(txt));
}
char CompilerPath[1024];
ExceptBuf SigPad;
void LexerTests() {
    //Comments
    CreateLexer(PARSER_HOLYC);
    char* hw=LexExpandText("/*Dummy*/Hello World//Dummy2\n");
    assert(0==strcmp(hw,"Hello World"));
    //
    CreateLexer(PARSER_HOLYC);
    char* ott=LexExpandText("#define x 123\nx");
    assert(0==strcmp(ott,"\n 123"));
    //
    CreateLexer(PARSER_HOLYC);
    char* inf=LexExpandText("#define x x\nx");
    //
    CreateLexer(PARSER_HOLYC);
    char *text="255 0xff 0377 25.5e1 \"ab\\\"c\"";
    AppendToLexer(text);
    AST *i=LexItem();
    assert(i->type==AST_INT);
    assert(i->integer==255);
    i=LexItem();
    assert(i->type==AST_INT);
    assert(i->integer==255);
    i=LexItem();
    assert(i->type==AST_INT);
    assert(i->integer==255);
    i=LexItem();
    assert(i->type==AST_FLOAT);
    assert(i->floating>=254);
    i=LexItem();
    assert(i->type==AST_STRING);
    assert(0==strcmp(i->string,"ab\"c"));

    CreateLexer(PARSER_HOLYC);
    text="'\\\'\\\"\\e\\n\\xff\\37a'";
    AppendToLexer(text);

    i=LexItem();
    assert(i->type==AST_STRING);
    assert(0==strcmp(i->string,"'\"\e\n\xff\37a"));

    char tmp1[L_tmpnam],tmp2[L_tmpnam];
    tmpnam(tmp1);
    FILE *f=fopen(tmp1,"wb");
    fprintf(f,"abc");
    fclose(f);
    tmpnam(tmp2);
    f=fopen(tmp2,"wb");
    fprintf(f,"#include \"%s\"",tmp1);
    fclose(f);

    CreateLexer(PARSER_HOLYC);
    char buffer[1024];
    sprintf(buffer,"#include \"%s\"",tmp2);
    char* inc=LexExpandText(buffer);
    assert(0==strcmp(inc,"abc"));

    CreateLexer(PARSER_HOLYC);
    text="if toad";
    AppendToLexer(text);

    i=LexItem(text);
    assert(i->type==AST_KEYWORD);
    assert(i->keyword==HC_IF);
    i=LexItem(text);
    assert(i->type==AST_NAME);
    assert(0==strcmp(i->name,"toad"));

    CreateLexer(PARSER_HOLYC);
    text="++ ( -";
    AppendToLexer(text);

    i=LexItem(text);
    assert(i->type==AST_TOKEN);
    assert(i->tokenAtom==HC_INC);
    i=LexItem(text);
    assert(i->type==AST_TOKEN);
    assert(i->tokenAtom==HC_LEFT_PAREN);
    i=LexItem(text);
    assert(i->type==AST_TOKEN);
    assert(i->tokenAtom==HC_SUB);
}
void ParserTests() {
    RegisterBuiltins();
    CreateLexer(PARSER_HOLYC);
    char* text="1+1./34;";
    AppendToLexer(text);
    HC_parse();
    //Compiler.debugMode=1;
    char *string="#include \"test/LEXER_T.HC\"";
    CreateLexer(PARSER_HOLYC);
    AppendToLexer(string);
    AttachParserToLexer();
    HC_parse();
    string="#include \"NESTED.HC\"";
    CreateLexer(PARSER_HOLYC);
    AppendToLexer(string);
    AttachParserToLexer();
    HC_parse();

    string="#include \"test/ERRORS.HC\"";
    CreateLexer(PARSER_HOLYC);
    AppendToLexer(string);
    AttachParserToLexer();
    //yyparse();
}
static void GenerateBinopTest(int64_t r,int64_t a,int64_t b,int type,CType *rtype,CType *atype,CType *btype) {
    int64_t globalptr;
    CVariable *global=TD_CALLOC(1,sizeof(CVariable));
    global->linkage.globalPtr=&globalptr;
    global->isGlobal=1;
    global->refCount=1;
    CVariable *localreg=TD_CALLOC(1,sizeof(CVariable));
    localreg->type=(atype);
    localreg->isReg=1;
    localreg->refCount=1;
    CVariable *localframe=TD_CALLOC(1,sizeof(CVariable));
    localframe->type=(btype);
    localframe->isNoreg=1;
    localframe->refCount=1;
    CVariable *cans[]= {global,localreg,localframe};
    int r1,r2,r3;
    for(r1=0; r1!=3; r1++) {
        for(r2=0; r2!=3; r2++) {
            for(r3=0; r3!=3; r3++) {
                if(r1==r2) continue;
                if(r1==r3) continue;
                if(r2==r3) continue;
                cans[r1]->type=(atype);
                cans[r2]->type=(btype);
                cans[r3]->type=(rtype);
                AST *stmts=AppendToStmts(NULL,CreateBinop(CreateVarNode(cans[r1]),CreateI64(a),AST_ASSIGN));
                stmts=AppendToStmts(stmts,CreateBinop(CreateVarNode(cans[r2]),CreateI64(b),AST_ASSIGN));
                stmts=AppendToStmts(stmts, CreateBinop(CreateVarNode(cans[r3]),
                                                       CreateBinop(
                                                               CreateVarNode(cans[r1]),
                                                               CreateVarNode(cans[r2]), type
                                                       ),AST_ASSIGN));
                stmts=AppendToStmts(stmts, CreateReturn(CreateVarNode(cans[r3])));
                map_CVariable_t locals;
                map_init(&locals);
                map_set(&locals, "r", localreg);
                map_set(&locals, "f", localframe);
                vec_CVariable_t unused;
                vec_init(&unused);
                Compiler.returnType=rtype;
                CFunction *f=CompileAST(&locals, stmts, unused,C_AST_FRAME_OFF_DFT,0);
                if(IsI64(rtype)) {
                    assert(((int64_t(*)())f->funcptr)(0)==r);
                } else if(IsF64(rtype)) {
                    double ret=((double(*)())f->funcptr)(0);
                    assert(r+.3>ret&&ret>r-.3);
                }
            }
        }
    }
    if(atype->type!=TYPE_PTR&&btype->type!=TYPE_PTR) {
        map_CVariable_t locals;
        map_init(&locals);
        map_set(&locals, "r", localreg);
        map_set(&locals, "f", localframe);
        vec_CVariable_t unused;
        vec_init(&unused);
        Compiler.returnType=CreatePrimType(TYPE_I64);
        CFunction *f=CompileAST(&locals, CreateReturn(CreateBinop(CreateI64(a), CreateI64(b), type)), unused,C_AST_FRAME_OFF_DFT,0);
        assert(((int64_t(*)())f->funcptr)(0)==r);
        switch(type) {
        case AST_ADD:
        case AST_SUB:
        case AST_DIV:
        case AST_MUL:
            Compiler.returnType=CreatePrimType(TYPE_F64);
            f=CompileAST(&locals, CreateReturn(CreateBinop(CreateF64(a), CreateF64(b), type)), unused,C_AST_FRAME_OFF_DFT,0);
            double ret=((double(*)())f->funcptr)(0);
            assert(r+.3>ret&&ret>r-.3);
        }
    }
}
static void GenerateUnopAssignTest(int64_t pre_r,int64_t post_r,int64_t a,int type,CType *t) {
    int64_t globalptri;
    CVariable *globali AF_VAR=TD_CALLOC(1,sizeof(CVariable));
    globali->type=(t);
    globali->linkage.globalPtr=&globalptri;
    globali->isGlobal=1;
    globali->refCount=1;
    double globalptrd;
    CVariable *globald AF_VAR=TD_CALLOC(1,sizeof(CVariable));
    globald->linkage.globalPtr=&globalptrd;
    globald->type=(t);
    globald->isGlobal=1;
    globald->refCount=1;

    CVariable *localreg AF_VAR=TD_CALLOC(1,sizeof(CVariable));
    localreg->type=(t);
    localreg->isReg=1;
    localreg->refCount=1;
    CVariable *localframe AF_VAR=TD_CALLOC(1,sizeof(CVariable));
    localframe->type=(t);
    localframe->isNoreg=1;
    localframe->refCount=1;
    CVariable *cans[]= {NULL,localreg,localframe};
    int r1;
    //+1 ignores global
    for(r1=+1; r1!=3; r1++) {
        AST *av AF_AST=CreateVarNode((IsI64(t))?globali:globald);
        AST *bv AF_AST=CreateVarNode(cans[r1]);
        AST *v AF_AST=CreateI64(a);
        AST *asn1 AF_AST=CreateBinop(av, v, AST_ASSIGN);
        AST *unop AF_AST=CreateUnop(av,type);
        AST *asn2 AF_AST=CreateBinop(bv,unop,AST_ASSIGN);
        AST *ret AF_AST=CreateReturn(asn2);
        AST *stmts AF_AST=AppendToStmts(NULL, asn1);
        stmts=AppendToStmts(stmts, ret);
        map_CVariable_t locals;
        map_init(&locals);
        map_set(&locals, "r", localreg);
        map_set(&locals, "f", localframe);
        vec_CVariable_t unused;
        vec_init(&unused);
        Compiler.returnType=t;
        CFunction *f=CompileAST(&locals, stmts, unused,C_AST_FRAME_OFF_DFT,0);
        if(IsI64(t)) {
            assert(((int64_t(*)())f->funcptr)(0)==pre_r);
            assert(globalptri==post_r);
        } else {
            assert(((double(*)())f->funcptr)(0)==pre_r);
            assert(globalptrd==post_r);
        }
        vec_deinit(&unused);
        map_deinit(&locals);
    }
}
static void GenerateUnopTest(int64_t r,int64_t a,int type,CType *rtype,CType *atype) {
    int64_t globalptr;
    CVariable *global AF_VAR=TD_CALLOC(1,sizeof(CVariable));
    global->linkage.globalPtr=&globalptr;
    global->isGlobal=1;
    global->refCount=1;
    CVariable *localreg AF_VAR=TD_CALLOC(1,sizeof(CVariable));
    localreg->type=(atype);
    localreg->isReg=1;
    localreg->refCount=1;
    CVariable *localframe AF_VAR=TD_CALLOC(1,sizeof(CVariable));
    localframe->type=(atype);
    localframe->isNoreg=1;
    localframe->refCount=1;
    CVariable *cans[]= {global,localreg,localframe};
    int r1,r2;
    for(r1=0; r1!=3; r1++) {
        for(r2=0; r2!=3; r2++) {
            if(r1==r2) continue;
            cans[r1]->type=(rtype);
            cans[r2]->type=(atype);
            AST *av AF_AST=CreateVarNode(cans[r2]);
            AST *rv AF_AST=CreateVarNode(cans[r1]);
            AST *unop AF_AST=CreateUnop(av, type);
            AST *a_asn AF_AST=CreateBinop(av,CreateI64(a), AST_ASSIGN);
            AST *asn AF_AST=CreateBinop(rv, unop, AST_ASSIGN);
            AST *ret AF_AST=CreateReturn(asn);
            AST *stmts AF_AST=AppendToStmts(NULL, a_asn);
            stmts=AppendToStmts(stmts, ret);
            map_CVariable_t locals;
            map_init(&locals);
            map_set(&locals, "r", localreg);
            map_set(&locals, "f", localframe);
            vec_CVariable_t unused;
            vec_init(&unused);
            Compiler.returnType=rtype;
            CFunction *f=CompileAST(&locals, stmts, unused,C_AST_FRAME_OFF_DFT,0);
            if(IsI64(rtype)) {
                assert(((int64_t(*)())f->funcptr)(0)==r);

            } else if(IsF64(rtype)) {
                double ret=((double(*)())f->funcptr)(0);
                assert(r+.3>ret&&ret>r-.3);
            }
            ReleaseFunction(f);
            map_deinit(&locals);
            vec_deinit(&unused);
        }
    }
}
static void GenerateRangeTests(CType *atype,CType *btype,CType *ctype) {
    int64_t i1;
    CVariable *global1 AF_VAR=TD_CALLOC(1,sizeof(CVariable));
    global1->linkage.globalPtr=&i1;
    global1->isGlobal=1;
    global1->refCount=1;
    global1->type=(atype);

    int64_t i2;
    CVariable *global2 AF_VAR=TD_CALLOC(1,sizeof(CVariable));
    global2->linkage.globalPtr=&i2;
    global2->isGlobal=1;
    global2->refCount=1;
    global2->type=(btype);

    int64_t i3;
    CVariable *global3 AF_VAR=TD_CALLOC(1,sizeof(CVariable));
    global3->linkage.globalPtr=&i3;
    global3->isGlobal=1;
    global3->refCount=1;
    global3->type=(ctype);
#define TEST(res,v1,v2,v3) \
  { \
    AST *a AF_AST=CreateBinop(CreateVarNode(global1), CreateI64(v1), AST_ASSIGN); \
    AST *b AF_AST=CreateBinop(CreateVarNode(global2), CreateI64(v2), AST_ASSIGN); \
    AST *c AF_AST=CreateBinop(CreateVarNode(global3), CreateI64(v3), AST_ASSIGN); \
    AST *stmts AF_AST=AppendToStmts(AppendToStmts(a, b),c); \
    AST *range AF_AST=AppendToRange(CreateVarNode(global1),CreateVarNode(global2), AST_LT); \
    range=AppendToRange(range, CreateVarNode(global3), AST_LT); \
    stmts=AppendToStmts(stmts,CreateReturn(range)); \
    map_CVariable_t locs; \
    map_init(&locs); \
    vec_CVariable_t unused; \
    vec_init(&unused); \
    CFunction *f=CompileAST(&locs,stmts,unused,C_AST_FRAME_OFF_DFT,0); \
    assert(((uint64_t(*)())f->funcptr)()==res); \
  }
    TEST(1,1,2,3);
    TEST(0,1,0,3);
}
void AssignModifyTests(int64_t r,int64_t a,int64_t b,int type,CType *rtype,CType *atype) {
    int64_t globalptr;
    CVariable *global=TD_CALLOC(1,sizeof(CVariable));
    global->linkage.globalPtr=&globalptr;
    global->isGlobal=1;
    global->refCount=1;
    CVariable *localreg=TD_CALLOC(1,sizeof(CVariable));
    localreg->type=(atype);
    localreg->isReg=1;
    localreg->refCount=1;
    CVariable *cans[]= {global,localreg};
    int r1,r2;
    for(r1=0; r1!=2; r1++) {
        for(r2=0; r2!=2; r2++) {
            if(r1==r2) continue;
            cans[r1]->type=(rtype);
            cans[r2]->type=(atype);
            AST *stmts=CreateBinop(CreateVarNode(cans[r1]), CreateI64(a), AST_ASSIGN);
            stmts=AppendToStmts(stmts, CreateBinop(CreateVarNode(cans[r2]), CreateI64(b), AST_ASSIGN));
            stmts=AppendToStmts(stmts,CreateReturn(CreateBinop(CreateVarNode(cans[r1]), CreateVarNode(cans[r2]), type)));
            map_CVariable_t locals;
            map_init(&locals);
            map_set(&locals,"r",localreg);
            vec_CVariable_t unused;
            vec_init(&unused);
            Compiler.returnType=rtype;
            CFunction *f=CompileAST(&locals,stmts,unused,C_AST_FRAME_OFF_DFT,0);
            if(IsI64(rtype)) {
                assert(((int64_t(*)())f->funcptr)(0)==r);
            } else if(IsF64(rtype)) {
                double ret=((double(*)())f->funcptr)(0);
                assert(r+.3>ret&&ret>r-.3);
            }
        }
    }
}
/*
static void GenerateAssignTypes() {
    int64_t  ptritem,iter;
    CVariable *ptrreg=TD_MALLOC(sizeof(CVariable));
    ptrreg->type=CreatePtrType(CreatePrimType(TYPE_I64));
    ptrreg->isReg=1;
    ptrreg->refCount=1;
    ptrreg->reg=2;
    CVariable *reg=TD_MALLOC(sizeof(CVariable));
    reg->type=CreatePrimType(TYPE_I64);
    reg->isReg=1;
    rreg->refCount=1;
    reg->reg=3;

    CValue ptrregv=VALUE_INDIR_VAR(ptrreg);
    CValue regv=VALUE_VAR(ptrreg);

    //Literal tests -> indirect
    CValue i=VALUE_INT(10);
    CValue s=VALUE_STRING("ABC");
    CValue cans1[]={i,s};
    for(iter=0;iter!=2;iter++) {
      struct jit *jit=jit_init();
      int64_t (*func)();
      jit_prolog(jit, &func);
      jit_movi(jit,R(ptrreg->reg),&ptritem);
      CompileAssign(ptrregv, cans1[iter]);
      jit_reti(jit, 0);
      ptritem=0;
      func();
      assert(ptritem==cans1[iter].integer);
    }
    //literal -> reg var
    for(iter=0;iter!=2;iter++) {
      struct jit *jit=jit_init();
      int64_t (*func)();
      jit_prolog(jit, &func);
      CompileAssign(reg, cans1[iter]);
      jit_retr(jit, R(reg->reg));
      ptritem=func();
      assert(ptritem==cans1[iter].integer);
    }

    CVariable *ptrreg2=TD_MALLOC(sizeof(CVariable));
    ptrreg2->type=CreatePtrType(CreatePrimType(TYPE_I64));
    ptrreg2->isReg=1;
    ptrreg2->refCount=1;
    ptrreg2->reg=4;
    CVariable *reg=TD_MALLOC(sizeof(CVariable));
    reg->type=CreatePrimType(TYPE_I64);
    reg->isReg=1;
    reg->refCount=1;
    reg->reg=5;
    CValue cans2[]={ptrregv,regv};
}*/
void CompileTests() {
    CType *i64=CreatePrimType(TYPE_I64);
    CType *i64p=CreatePtrType(i64);
    CType *f64=CreatePrimType(TYPE_F64);
#define ALL_UPERMS(r,a,op) \
  GenerateUnopTest(r,a,op,i64,i64); \
  GenerateUnopTest(r,a,op,f64,f64); \
  GenerateUnopTest(r,a,op,i64,f64); \
  GenerateUnopTest(r,a,op,f64,i64);


#define ALL_TPERMS(r,a,b,op) \
  GenerateBinopTest(r,a,b,op,i64,i64,i64); \
  GenerateBinopTest(r,a,b,op,f64,i64,i64); \
  GenerateBinopTest(r,a,b,op,f64,f64,i64); \
  GenerateBinopTest(r,a,b,op,f64,f64,f64); \
  GenerateBinopTest(r,a,b,op,i64,f64,f64); \
  GenerateBinopTest(r,a,b,op,i64,f64,i64);

    //Test integer bitwise operators
    GenerateBinopTest(3, 1, 2, AST_BOR, i64,i64,i64);
    GenerateBinopTest(1, 3, 2, AST_BXOR, i64,i64,i64);
    GenerateBinopTest(2, 3, 2, AST_BAND, i64,i64,i64);

    ALL_TPERMS(3,15,5,AST_DIV);
    ALL_TPERMS(15,15,5,AST_COMMA);
    ALL_TPERMS(8,2,3,AST_POW);
    ALL_TPERMS(6,2,3,AST_MUL);
    ALL_TPERMS(3,15,6,AST_MOD);
    ALL_TPERMS(21,15,6,AST_ADD);
    ALL_TPERMS(9,15,6,AST_SUB);
    ALL_TPERMS(0,15,6,AST_LT);
    ALL_TPERMS(1,15,6,AST_GT);
    ALL_TPERMS(0,15,6,AST_LE);
    ALL_TPERMS(1,15,6,AST_GE);
    ALL_TPERMS(1,1,1,AST_EQ);
    ALL_TPERMS(1,2,1,AST_NE);

    ALL_TPERMS(1,2,1,AST_LAND);
    ALL_TPERMS(0,2,0,AST_LAND);

    ALL_TPERMS(1,0,1,AST_LOR);
    ALL_TPERMS(0,0,0,AST_LOR);

    ALL_TPERMS(1,0,1,AST_LXOR);
    ALL_TPERMS(0,1,1,AST_LXOR);
    ALL_TPERMS(0,0,0,AST_LXOR);

    GenerateRangeTests(i64,i64,i64);
    GenerateRangeTests(f64,f64,f64);
    GenerateRangeTests(f64,i64,f64);
#define ALL_MOD_TPERMS(r,a,b,t) \
  AssignModifyTests(r, a,b, t, i64, i64); \
  AssignModifyTests(r, a,b, t, i64, f64); \
  AssignModifyTests(r, a,b, t, f64, f64);
    ALL_MOD_TPERMS(10, 5, 2, AST_ASSIGN_MUL);
    ALL_MOD_TPERMS(1, 5, 2, AST_ASSIGN_MOD);
    ALL_MOD_TPERMS(5, 10, 2, AST_ASSIGN_DIV);
    ALL_MOD_TPERMS(7, 5, 2, AST_ASSIGN_ADD);
    ALL_MOD_TPERMS(8, 10, 2, AST_ASSIGN_SUB);

    GenerateBinopTest(88,8,10,AST_ADD,i64p,i64p,i64);
    GenerateBinopTest(88,8,10,AST_ADD,i64p,i64,i64p);

    GenerateBinopTest(10,88,8,AST_SUB,i64,i64p,i64p);
    GenerateBinopTest(8,88,10,AST_SUB,i64p,i64p,i64);

    ALL_UPERMS(1, 0, AST_LNOT);
    ALL_UPERMS(0, 1, AST_LNOT);
    ALL_UPERMS(-1, 1, AST_NEG);
    ALL_UPERMS(1, 1, AST_POS);
    GenerateUnopTest(-2, 1, AST_BNOT, i64,i64);

    GenerateUnopAssignTest(1, 1,0, AST_PRE_INC, i64);
    GenerateUnopAssignTest(1, 1,0, AST_PRE_INC, f64);
    //
    GenerateUnopAssignTest(1, 1,2, AST_PRE_DEC, i64);
    GenerateUnopAssignTest(1, 1,2, AST_PRE_DEC, f64);
    //
    GenerateUnopAssignTest(2, 1,2, AST_POST_DEC, i64);
    GenerateUnopAssignTest(2, 1,2, AST_POST_DEC, f64);
    //
    GenerateUnopAssignTest(0, 1,0, AST_POST_INC, i64);
    GenerateUnopAssignTest(0, 1,0, AST_POST_INC, f64);
}
int main() {
    CreateGC(__builtin_frame_address(0), 0);
    AddGCRoot(&Compiler,sizeof(Compiler));
    AddGCRoot(&Lexer,sizeof(Lexer));
    //AddGCRoot(&Debugger,sizeof(Debugger));
    CreateLexer(PARSER_HOLYC);
    // LexerTests();
    //Compiler.debugMode=1;
    ParserTests();
    //CompileTests();
    return 0;
}
