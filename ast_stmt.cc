/* File: ast_stmt.cc
 * -----------------
 * Implementation of statement node classes.
 */

#include "ast_stmt.h"
#include "ast_type.h"
#include "ast_decl.h"
#include "ast_expr.h"
#include "codegen.h"
#include "hashtable.h"

Scope *Program::gScope = new Scope;
stack<const char*> *Program::gBreakLabels = new stack<const char*>;

Scope::Scope() : table(new Hashtable<Decl*>) {
    // Empty
}

/* XXX: Only semantically valid programs will be tested, thus no semantic
 * checking is performed here.
 */
void Scope::AddDecl(Decl *d) {
    table->Enter(d->GetName(), d);
}

ostream& operator<<(ostream& out, Scope *s) {
    out << "========== Scope ==========" << std::endl;
    Iterator<Decl*> iter = s->table->GetIterator();
    Decl *d;
    while ((d = iter.GetNextValue()) != NULL)
        out << d << std::endl;
    return out;
}

Program::Program(List<Decl*> *d) : codeGenerator(new CodeGenerator) {
    Assert(d != NULL);
    (decls=d)->SetParentAll(this);
    scope = gScope;
}

void Program::Check() {
    /* You can use your pp3 semantic analysis or leave it out if
     * you want to avoid the clutter.  We won't test pp4 against
     * semantically-invalid programs.
     */
    for (int i = 0, n = decls->NumElements(); i < n; ++i)
        gScope->AddDecl(decls->Nth(i));

    for (int i = 0, n = decls->NumElements(); i < n; ++i)
        decls->Nth(i)->BuildScope();

    /* XXX: Only semantically valid programs will be tested, thus no
     * semantic checking is performed here.
     */
}

void Program::Emit() {
    /* pp4: here is where the code generation is kicked off.
     *      The general idea is perform a tree traversal of the
     *      entire program, generating instructions as you go.
     *      Each node can have its own way of translating itself,
     *      which makes for a great use of inheritance and
     *      polymorphism in the node classes.
     */
    int offset = CodeGenerator::OffsetToFirstGlobal;

    for (int i = 0, n = decls->NumElements(); i < n; ++i) {
        Decl *d = decls->Nth(i);
        Location *loc = new Location(gpRelative, offset, d->GetName());
        if (d->SetMemLoc(loc) == 0)
            offset += CodeGenerator::VarSize;
    }

    for (int i = 0, n = decls->NumElements(); i < n; ++i)
        decls->Nth(i)->Emit(codeGenerator);

    codeGenerator->DoFinalCodeGen();
}

Stmt::Stmt() : Node() {
    scope = new Scope;
}

Stmt::Stmt(yyltype loc) : Node(loc) {
    scope = new Scope;
}

StmtBlock::StmtBlock(List<VarDecl*> *d, List<Stmt*> *s) {
    Assert(d != NULL && s != NULL);
    (decls=d)->SetParentAll(this);
    (stmts=s)->SetParentAll(this);
}

void StmtBlock::BuildScope() {
    for (int i = 0, n = decls->NumElements(); i < n; ++i)
        scope->AddDecl(decls->Nth(i));

    for (int i = 0, n = decls->NumElements(); i < n; ++i)
        decls->Nth(i)->BuildScope();

    for (int i = 0, n = stmts->NumElements(); i < n; ++i)
        stmts->Nth(i)->BuildScope();
}

Location* StmtBlock::Emit(CodeGenerator *cg) {
    int offset = CodeGenerator::OffsetToFirstLocal;

    for (int i = 0, n = decls->NumElements(); i < n; ++i) {
        Decl *d = decls->Nth(i);
        Location *loc = new Location(fpRelative, offset, d->GetName());
        if (d->SetMemLoc(loc) == 0)
            offset -= d->GetMemBytes();
    }

    cg->SetLocalOffset(offset);

    for (int i = 0, n = stmts->NumElements(); i < n; ++i)
        stmts->Nth(i)->Emit(cg);

    return NULL;
}

int StmtBlock::GetMemBytes() {
    int memBytes = 0;

    for (int i = 0, n = decls->NumElements(); i < n; ++i)
        memBytes += decls->Nth(i)->GetMemBytes();

    for (int i = 0, n = stmts->NumElements(); i < n; ++i)
        memBytes += stmts->Nth(i)->GetMemBytes();

    return memBytes;
}

ConditionalStmt::ConditionalStmt(Expr *t, Stmt *b) {
    Assert(t != NULL && b != NULL);
    (test=t)->SetParent(this);
    (body=b)->SetParent(this);
}

void ConditionalStmt::BuildScope() {
    test->BuildScope();
    body->BuildScope();
}

void LoopStmt::BuildScope() {
    ConditionalStmt::BuildScope();
}

ForStmt::ForStmt(Expr *i, Expr *t, Expr *s, Stmt *b): LoopStmt(t, b) {
    Assert(i != NULL && t != NULL && s != NULL && b != NULL);
    (init=i)->SetParent(this);
    (step=s)->SetParent(this);
}

void ForStmt::BuildScope() {
    LoopStmt::BuildScope();

    init->BuildScope();
    step->BuildScope();
}

Location* ForStmt::Emit(CodeGenerator *cg) {
    const char* top = cg->NewLabel();
    const char* bot = cg->NewLabel();

    Program::gBreakLabels->push(bot);

    init->Emit(cg);
    cg->GenLabel(top);
    Location *t = test->Emit(cg);
    cg->GenIfZ(t, bot);
    body->Emit(cg);
    step->Emit(cg);
    cg->GenGoto(top);
    cg->GenLabel(bot);

    Program::gBreakLabels->pop();

    return NULL;

}

int ForStmt::GetMemBytes() {
    return init->GetMemBytes() + test->GetMemBytes() +
           body->GetMemBytes() + step->GetMemBytes();
}

void WhileStmt::BuildScope() {
    LoopStmt::BuildScope();
}

Location* WhileStmt::Emit(CodeGenerator *cg) {
    const char* top = cg->NewLabel();
    const char* bot = cg->NewLabel();

    Program::gBreakLabels->push(bot);

    cg->GenLabel(top);
    Location *t = test->Emit(cg);
    cg->GenIfZ(t, bot);
    body->Emit(cg);
    cg->GenGoto(top);
    cg->GenLabel(bot);

    Program::gBreakLabels->pop();

    return NULL;
}

int WhileStmt::GetMemBytes() {
    return test->GetMemBytes() + body->GetMemBytes();
}

IfStmt::IfStmt(Expr *t, Stmt *tb, Stmt *eb): ConditionalStmt(t, tb) {
    Assert(t != NULL && tb != NULL); // else can be NULL
    elseBody = eb;
    if (elseBody) elseBody->SetParent(this);
}

void IfStmt::BuildScope() {
    ConditionalStmt::BuildScope();

    if (elseBody) elseBody->BuildScope();
}

Location* IfStmt::Emit(CodeGenerator *cg) {
    const char* els = cg->NewLabel();
    const char* bot = cg->NewLabel();

    Location *t = test->Emit(cg);
    cg->GenIfZ(t, els);
    body->Emit(cg);
    cg->GenGoto(bot);
    cg->GenLabel(els);
    if (elseBody) elseBody->Emit(cg);
    cg->GenLabel(bot);

    return NULL;
}

int IfStmt::GetMemBytes() {
    int memBytes = test->GetMemBytes() + body->GetMemBytes();
    if (elseBody) memBytes += elseBody->GetMemBytes();
    return memBytes;
}

Location* BreakStmt::Emit(CodeGenerator *cg) {
    cg->GenGoto(Program::gBreakLabels->top());
    return NULL;
}

int BreakStmt::GetMemBytes() {
    return 0;
}

ReturnStmt::ReturnStmt(yyltype loc, Expr *e) : Stmt(loc) {
    Assert(e != NULL);
    (expr=e)->SetParent(this);
}

void ReturnStmt::BuildScope() {
    expr->BuildScope();
}

Location* ReturnStmt::Emit(CodeGenerator *cg) {
    if (expr == NULL)
        cg->GenReturn();
    else
        cg->GenReturn(expr->Emit(cg));

    return NULL;
}

int ReturnStmt::GetMemBytes() {
    if (expr == NULL)
        return 0;
    else
        return expr->GetMemBytes();
}

PrintStmt::PrintStmt(List<Expr*> *a) {
    Assert(a != NULL);
    (args=a)->SetParentAll(this);
}

void PrintStmt::BuildScope() {
    for (int i = 0, n = args->NumElements(); i < n; ++i)
        args->Nth(i)->BuildScope();
}

Location* PrintStmt::Emit(CodeGenerator *cg) {
    for (int i = 0, n = args->NumElements(); i < n; ++i) {
        Expr *e = args->Nth(i);
        Type *t = e->GetType();
        BuiltIn b = NumBuiltIns;
        if (t == Type::intType)
            b = PrintInt;
        else if (t == Type::boolType)
            b = PrintBool;
        else if (t == Type::stringType)
            b = PrintString;
        else
            /* Print can only take ints, bools, or strings as parameters
             * (remember, doubles need not be supported for PP4). Thus, we
             * should never reach this Assert statement.
             */
            Assert(0);

        cg->GenBuiltInCall(b, e->Emit(cg));
    }

    return NULL;
}

int PrintStmt::GetMemBytes() {
    int memBytes = 0;
    for (int i = 0, n = args->NumElements(); i < n; ++i)
        memBytes += args->Nth(i)->GetMemBytes();
   return memBytes;
}
