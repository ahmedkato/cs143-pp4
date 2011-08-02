/* File: ast_expr.cc
 * -----------------
 * Implementation of expression node classes.
 */

#include <string.h>
#include "ast_expr.h"
#include "ast_type.h"
#include "ast_decl.h"
#include "codegen.h"

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

Location* IntConstant::Emit(CodeGenerator *cg) {
    return cg->GenLoadConstant(value);
}

DoubleConstant::DoubleConstant(yyltype loc, double val) : Expr(loc) {
    value = val;
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

Location* StringConstant::Emit(CodeGenerator *cg) {
    return cg->GenLoadConstant(value);
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

Location* ArithmeticExpr::Emit(CodeGenerator *cg) {
    return CompoundExpr::Emit(cg);
}

Location* RelationalExpr::Emit(CodeGenerator *cg) {
    return CompoundExpr::Emit(cg);
}

Location* EqualityExpr::Emit(CodeGenerator *cg) {
    return CompoundExpr::Emit(cg);
}

Location* LogicalExpr::Emit(CodeGenerator *cg) {
    return CompoundExpr::Emit(cg);
}

Location* AssignExpr::Emit(CodeGenerator *cg) {
    Location *ltemp = left->Emit(cg);
    Location *rtemp = right->Emit(cg);
    cg->GenAssign(ltemp, rtemp);
    return NULL;
}

ArrayAccess::ArrayAccess(yyltype loc, Expr *b, Expr *s) : LValue(loc) {
    (base=b)->SetParent(this);
    (subscript=s)->SetParent(this);
}

FieldAccess::FieldAccess(Expr *b, Identifier *f)
  : LValue(b? Join(b->GetLocation(), f->GetLocation()) : *f->GetLocation()) {
    Assert(f != NULL); // b can be be NULL (just means no explicit base)
    base = b;
    if (base) base->SetParent(this);
    (field=f)->SetParent(this);
}

Location* FieldAccess::Emit(CodeGenerator *cg) {
    VarDecl *d = GetDecl();
    if (d == NULL)
        return NULL;

    return d->GetMemLoc();
}

VarDecl* FieldAccess::GetDecl() {
    if (base != NULL) return NULL; // TODO: Support cases when base != NULL

    ClassDecl *classDecl = GetClassDecl();

    if (classDecl == NULL) {
        Node *n = this;
        while (n != NULL) {
            Decl *d = n->GetScope()->table->Lookup(field->GetName());
            if (d != NULL)
                return dynamic_cast<VarDecl*>(d);
            n = n->GetParent();
        }
        return NULL;
    }

    Decl *d = classDecl->GetScope()->table->Lookup(field->GetName());
    return dynamic_cast<VarDecl*>(d);

}

Call::Call(yyltype loc, Expr *b, Identifier *f, List<Expr*> *a) : Expr(loc)  {
    Assert(f != NULL && a != NULL); // b can be be NULL (just means no explicit base)
    base = b;
    if (base) base->SetParent(this);
    (field=f)->SetParent(this);
    (actuals=a)->SetParentAll(this);
}

NewExpr::NewExpr(yyltype loc, NamedType *c) : Expr(loc) {
  Assert(c != NULL);
  (cType=c)->SetParent(this);
}

NewArrayExpr::NewArrayExpr(yyltype loc, Expr *sz, Type *et) : Expr(loc) {
    Assert(sz != NULL && et != NULL);
    (size=sz)->SetParent(this);
    (elemType=et)->SetParent(this);
}
