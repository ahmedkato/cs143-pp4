/* File: ast_stmt.cc
 * -----------------
 * Implementation of statement node classes.
 */

#include "ast_stmt.h"
#include "ast_type.h"
#include "ast_decl.h"
#include "ast_expr.h"
#include "codegen.h"

Scope *Program::gScope = new Scope;

Scope::Scope() : table(new Hashtable<Decl*>) {
    // Empty
}

/* XXX: Only semantically valid programs will be tested, thus no semantic
 * checking is performed here.
 */
void Scope::AddDecl(Decl *d) {
    table->Enter(d->GetName(), d);
}

Program::Program(List<Decl*> *d) : codeGenerator(new CodeGenerator) {
    Assert(d != NULL);
    (decls=d)->SetParentAll(this);
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
    for (int i = 0, n = decls->NumElements(); i < n; ++i)
        decls->Nth(i)->Emit(codeGenerator);

    codeGenerator->DoFinalCodeGen();
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
    for (int i = 0, n = stmts->NumElements(); i < n; ++i)
        stmts->Nth(i)->Emit(cg);

    return NULL;
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

void WhileStmt::BuildScope() {
    LoopStmt::BuildScope();
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

ReturnStmt::ReturnStmt(yyltype loc, Expr *e) : Stmt(loc) {
    Assert(e != NULL);
    (expr=e)->SetParent(this);
}

void ReturnStmt::BuildScope() {
    expr->BuildScope();
}

PrintStmt::PrintStmt(List<Expr*> *a) {
    Assert(a != NULL);
    (args=a)->SetParentAll(this);
}

void PrintStmt::BuildScope() {
    for (int i = 0, n = args->NumElements(); i < n; ++i)
        args->Nth(i)->BuildScope();
}
