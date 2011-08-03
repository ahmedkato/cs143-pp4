/* File: ast_expr.cc
 * -----------------
 * Implementation of expression node classes.
 */

#include <string.h>
#include "ast_expr.h"
#include "ast_type.h"
#include "ast_decl.h"
#include "codegen.h"

Decl* Expr::GetFieldDecl(Identifier *field, Expr *b) {
    ClassDecl *classDecl = GetClassDecl();

    Decl *d;
    if (b != NULL)
        d = GetFieldDecl(field, b->GetType());
    else if (classDecl != NULL)
        d = GetFieldDecl(field, static_cast<Node*>(classDecl));
    else
        d = GetFieldDecl(field, static_cast<Node*>(this));

    return d;
}

Decl* Expr::GetFieldDecl(Identifier *field, Node *n) {
    while (n != NULL) {
        Decl *d = n->GetScope()->table->Lookup(field->GetName());
        if (d != NULL)
            return d;
        n = n->GetParent();
    }
    return NULL;
}

Decl* Expr::GetFieldDecl(Identifier *field, Type *t) {
    // If t != NamedType then this sets t = NULL
    t = dynamic_cast<NamedType*>(t);

    while (t != NULL) {
        Decl *tDecl = Program::gScope->table->Lookup(t->GetName());
        Decl *d = GetFieldDecl(field, tDecl);
        if (d != NULL)
            return d;

        if (dynamic_cast<ClassDecl*>(tDecl))
            t = static_cast<ClassDecl*>(tDecl)->GetExtends();
        else
            break;
    }
    return NULL;
}

ClassDecl* Expr::GetClassDecl() {
    Node *n = this;
    while (n != NULL) {
        if (dynamic_cast<ClassDecl*>(n))
            return static_cast<ClassDecl*>(n);
        n = n->GetParent();
    }
    return NULL;
}

IntConstant::IntConstant(yyltype loc, int val) : Expr(loc) {
    value = val;
}

Type* IntConstant::GetType() {
    return Type::intType;
}

Location* IntConstant::Emit(CodeGenerator *cg) {
    return cg->GenLoadConstant(value);
}

DoubleConstant::DoubleConstant(yyltype loc, double val) : Expr(loc) {
    value = val;
}

Type* DoubleConstant::GetType() {
    return Type::doubleType;
}

Location* DoubleConstant::Emit(CodeGenerator *cg) {
    /* From the PP4 assignment description (page 3):
     * "We have removed doubles from the type of Decaf for this project"
     * Thus, Assert if this is encountered.
     */
    Assert(0);
    return NULL;
}

BoolConstant::BoolConstant(yyltype loc, bool val) : Expr(loc) {
    value = val;
}

Type* BoolConstant::GetType() {
    return Type::boolType;
}

Location* BoolConstant::Emit(CodeGenerator *cg) {
    /* From the PP4 assignment description (page 3):
     * "We treat bools just like ordinary 4-byte integers, which evaluate to 0
     * or not 0.
     */
    return cg->GenLoadConstant(value ? 1 : 0);
}

StringConstant::StringConstant(yyltype loc, const char *val) : Expr(loc) {
    Assert(val != NULL);
    value = strdup(val);
}

Type* StringConstant::GetType() {
    return Type::stringType;
}

Location* StringConstant::Emit(CodeGenerator *cg) {
    return cg->GenLoadConstant(value);
}

Type* NullConstant::GetType() {
    return Type::nullType;
}

Location* NullConstant::Emit(CodeGenerator *cg) {
    return cg->GenLoadConstant(0);
}

Operator::Operator(yyltype loc, const char *tok) : Node(loc) {
    Assert(tok != NULL);
    strncpy(tokenString, tok, sizeof(tokenString));
}

CompoundExpr::CompoundExpr(Expr *l, Operator *o, Expr *r)
  : Expr(Join(l->GetLocation(), r->GetLocation())) {
    Assert(l != NULL && o != NULL && r != NULL);
    (op=o)->SetParent(this);
    (left=l)->SetParent(this);
    (right=r)->SetParent(this);
}

CompoundExpr::CompoundExpr(Operator *o, Expr *r)
  : Expr(Join(o->GetLocation(), r->GetLocation())) {
    Assert(o != NULL && r != NULL);
    left = NULL;
    (op=o)->SetParent(this);
    (right=r)->SetParent(this);
}

Location* CompoundExpr::Emit(CodeGenerator *cg) {
    Location *ltemp = left->Emit(cg);
    Location *rtemp = right->Emit(cg);

    // TODO: Simulate the unsupported operators
    return cg->GenBinaryOp(op->GetTokenString(), ltemp, rtemp);
}

Type* ArithmeticExpr::GetType() {
    return right->GetType();
}

Location* ArithmeticExpr::Emit(CodeGenerator *cg) {
    return CompoundExpr::Emit(cg);
}

Type* RelationalExpr::GetType() {
    return Type::boolType;
}

Location* RelationalExpr::Emit(CodeGenerator *cg) {
    return CompoundExpr::Emit(cg);
}

Type* EqualityExpr::GetType() {
    return Type::boolType;
}

Location* EqualityExpr::Emit(CodeGenerator *cg) {
    return CompoundExpr::Emit(cg);
}

Type* LogicalExpr::GetType() {
    return Type::boolType;
}

Location* LogicalExpr::Emit(CodeGenerator *cg) {
    return CompoundExpr::Emit(cg);
}

Type* AssignExpr::GetType() {
    return left->GetType();
}

Location* AssignExpr::Emit(CodeGenerator *cg) {
    Location *ltemp = left->Emit(cg);
    Location *rtemp = right->Emit(cg);
    cg->GenAssign(ltemp, rtemp);
    return NULL;
}

Type* This::GetType() {
    ClassDecl *d = GetClassDecl();
    if (d == NULL)
        return NULL;
    return d->GetType();
}

ArrayAccess::ArrayAccess(yyltype loc, Expr *b, Expr *s) : LValue(loc) {
    (base=b)->SetParent(this);
    (subscript=s)->SetParent(this);
}

Type* ArrayAccess::GetType() {
    return base->GetType();
}

FieldAccess::FieldAccess(Expr *b, Identifier *f)
  : LValue(b? Join(b->GetLocation(), f->GetLocation()) : *f->GetLocation()) {
    Assert(f != NULL); // b can be be NULL (just means no explicit base)
    base = b;
    if (base) base->SetParent(this);
    (field=f)->SetParent(this);
}

Type* FieldAccess::GetType() {
    VarDecl *d = GetDecl();
    if (d == NULL)
        return NULL;
    return d->GetType();
}

Location* FieldAccess::Emit(CodeGenerator *cg) {
    VarDecl *d = GetDecl();
    if (d == NULL)
        return NULL;

    return d->GetMemLoc();
}

VarDecl* FieldAccess::GetDecl() {
    Decl *d = GetFieldDecl(field, base);
    return dynamic_cast<VarDecl*>(d);

}

Call::Call(yyltype loc, Expr *b, Identifier *f, List<Expr*> *a) : Expr(loc)  {
    Assert(f != NULL && a != NULL); // b can be be NULL (just means no explicit base)
    base = b;
    if (base) base->SetParent(this);
    (field=f)->SetParent(this);
    (actuals=a)->SetParentAll(this);
}

Type* Call::GetType() {
    if (IsArrayLengthCall())
        return Type::intType;

    FnDecl *d = GetDecl();
    if (d == NULL)
        return NULL;
    return d->GetType();
}

FnDecl* Call::GetDecl() {
    Decl *d = GetFieldDecl(field, base);
    return dynamic_cast<FnDecl*>(d);
}

bool Call::IsArrayLengthCall() {
    if (base == NULL)
        return false;

    if (dynamic_cast<ArrayType*>(base->GetType()) == NULL)
        return false;

    if (strcmp("length", field->GetName()) != 0)
        return false;

    return true;
}

NewExpr::NewExpr(yyltype loc, NamedType *c) : Expr(loc) {
  Assert(c != NULL);
  (cType=c)->SetParent(this);
}

Type* NewExpr::GetType() {
    Decl *d = Program::gScope->table->Lookup(cType->GetName());
    ClassDecl *c = dynamic_cast<ClassDecl*>(d);
    if (c == NULL)
        return NULL;
    return c->GetType();
}

NewArrayExpr::NewArrayExpr(yyltype loc, Expr *sz, Type *et) : Expr(loc) {
    Assert(sz != NULL && et != NULL);
    (size=sz)->SetParent(this);
    (elemType=et)->SetParent(this);
}

Type* NewArrayExpr::GetType() {
    return new ArrayType(elemType);
}

Type* ReadIntegerExpr::GetType() {
    return Type::intType;
}

Type* ReadLineExpr::GetType() {
    return Type::stringType;
}
