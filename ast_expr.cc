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

int IntConstant::GetMemBytes() {
    return 4;
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

int DoubleConstant::GetMemBytes() {
    return 4;
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

int BoolConstant::GetMemBytes() {
    return 4;
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

int StringConstant::GetMemBytes() {
    return 4;
}

Type* NullConstant::GetType() {
    return Type::nullType;
}

Location* NullConstant::Emit(CodeGenerator *cg) {
    return cg->GenLoadConstant(0);
}

int NullConstant::GetMemBytes() {
    return 4;
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

Type* ArithmeticExpr::GetType() {
    return right->GetType();
}

Location* ArithmeticExpr::Emit(CodeGenerator *cg) {
    if (left == NULL)
        return EmitUnary(cg);
    else
        return EmitBinary(cg);
}

int ArithmeticExpr::GetMemBytes() {
    if (left == NULL)
        return GetMemBytesUnary();
    else
        return GetMemBytesBinary();
}

Location* ArithmeticExpr::EmitUnary(CodeGenerator *cg) {
    Location *rtemp = right->Emit(cg);

    Location *zero = cg->GenLoadConstant(0);
    return cg->GenBinaryOp(op->GetTokenString(), zero, rtemp);
}

int ArithmeticExpr::GetMemBytesUnary() {
    return right->GetMemBytes() + 2 * CodeGenerator::VarSize;
}

Location* ArithmeticExpr::EmitBinary(CodeGenerator *cg) {
    Location *ltemp = left->Emit(cg);
    Location *rtemp = right->Emit(cg);

    return cg->GenBinaryOp(op->GetTokenString(), ltemp, rtemp);
}

int ArithmeticExpr::GetMemBytesBinary() {
    return right->GetMemBytes() + left->GetMemBytes() + CodeGenerator::VarSize;
}

Type* RelationalExpr::GetType() {
    return Type::boolType;
}

Location* RelationalExpr::Emit(CodeGenerator *cg) {
    const char *tok = op->GetTokenString();

    if (strcmp("<", tok) == 0)
        return EmitLess(cg, left, right);
    else if (strcmp("<=", tok) == 0)
        return EmitLessEqual(cg, left, right);
    else if (strcmp(">", tok) == 0)
        return EmitLess(cg, right, left);
    else if (strcmp(">=", tok) == 0)
        return EmitLessEqual(cg, right, left);
    else
        Assert(0); // Should never reach this point!

    return NULL;
}

int RelationalExpr::GetMemBytes() {
    const char *tok = op->GetTokenString();

    if (strcmp("<", tok) == 0)
        return GetMemBytesLess(left, right);
    else if (strcmp("<=", tok) == 0)
        return GetMemBytesLessEqual(left, right);
    else if (strcmp(">", tok) == 0)
        return GetMemBytesLess(right, left);
    else if (strcmp(">=", tok) == 0)
        return GetMemBytesLessEqual(right, left);
    else
        Assert(0); // Should never reach this point!

    return 0;
}

Location* RelationalExpr::EmitLess(CodeGenerator *cg, Expr *l, Expr *r) {
    Location *ltmp = l->Emit(cg);
    Location *rtmp = r->Emit(cg);

    return cg->GenBinaryOp("<", ltmp, rtmp);
}

int RelationalExpr::GetMemBytesLess(Expr *l, Expr *r) {
    return l->GetMemBytes() + r->GetMemBytes() + CodeGenerator::VarSize;
}

Location* RelationalExpr::EmitLessEqual(CodeGenerator *cg, Expr *l, Expr *r) {
    Location *ltmp = l->Emit(cg);
    Location *rtmp = r->Emit(cg);

    Location *less = cg->GenBinaryOp("<", ltmp, rtmp);
    Location *equal = cg->GenBinaryOp("==", ltmp, rtmp);

    return cg->GenBinaryOp("||", less, equal);
}

int RelationalExpr::GetMemBytesLessEqual(Expr *l, Expr *r) {
    return l->GetMemBytes() + r->GetMemBytes() + 3 * CodeGenerator::VarSize;
}

Type* EqualityExpr::GetType() {
    return Type::boolType;
}

Location* EqualityExpr::Emit(CodeGenerator *cg) {
    const char *tok = op->GetTokenString();

    if (strcmp("==", tok) == 0)
        return EmitEqual(cg);
    else if (strcmp("!=", tok) == 0)
        return EmitNotEqual(cg);
    else
        Assert(0); // Should never reach this point!

    return NULL;
}

int EqualityExpr::GetMemBytes() {
    const char *tok = op->GetTokenString();

    if (strcmp("==", tok) == 0)
        return GetMemBytesEqual();
    else if (strcmp("!=", tok) == 0)
        return GetMemBytesNotEqual();
    else
        Assert(0); // Should never reach this point!

    return 0;
}

Location* EqualityExpr::EmitEqual(CodeGenerator *cg) {
    Location *ltmp = left->Emit(cg);
    Location *rtmp = right->Emit(cg);

    return cg->GenBinaryOp("==", ltmp, rtmp);
}

int EqualityExpr::GetMemBytesEqual() {
    return left->GetMemBytes() + right->GetMemBytes() + CodeGenerator::VarSize;
}

Location* EqualityExpr::EmitNotEqual(CodeGenerator *cg) {
    const char* ret_zro = cg->NewLabel();
    const char* ret_one = cg->NewLabel();
    Location *ret = cg->GenTempVar();

    Location *ltmp = left->Emit(cg);
    Location *rtmp = right->Emit(cg);

    Location *equal = cg->GenBinaryOp("==", ltmp, rtmp);

    cg->GenIfZ(equal, ret_one);
    cg->GenAssign(ret, cg->GenLoadConstant(0));
    cg->GenGoto(ret_zro);
    cg->GenLabel(ret_one);
    cg->GenAssign(ret, cg->GenLoadConstant(1));
    cg->GenLabel(ret_zro);

    return ret;
}

int EqualityExpr::GetMemBytesNotEqual() {
    return left->GetMemBytes() + right->GetMemBytes() +
           4 * CodeGenerator::VarSize;
}

Type* LogicalExpr::GetType() {
    return Type::boolType;
}

Location* LogicalExpr::Emit(CodeGenerator *cg) {
    const char *tok = op->GetTokenString();

    if (strcmp("&&", tok) == 0)
        return EmitAnd(cg);
    else if (strcmp("||", tok) == 0)
        return EmitOr(cg);
    else if (strcmp("!", tok) == 0)
        return EmitNot(cg);
    else
        Assert(0); // Should never reach this point!

    return 0;
}

int LogicalExpr::GetMemBytes() {
    const char *tok = op->GetTokenString();

    if (strcmp("&&", tok) == 0)
        return GetMemBytesAnd();
    else if (strcmp("||", tok) == 0)
        return GetMemBytesOr();
    else if (strcmp("!", tok) == 0)
        return GetMemBytesNot();
    else
        Assert(0); // Should never reach this point!

    return 0;
}

Location* LogicalExpr::EmitAnd(CodeGenerator *cg) {
    Location *ltmp = left->Emit(cg);
    Location *rtmp = right->Emit(cg);

    return cg->GenBinaryOp("&&", ltmp, rtmp);
}

int LogicalExpr::GetMemBytesAnd() {
    return left->GetMemBytes() + right->GetMemBytes() + CodeGenerator::VarSize;
}

Location* LogicalExpr::EmitOr(CodeGenerator *cg) {
    Location *ltmp = left->Emit(cg);
    Location *rtmp = right->Emit(cg);

    return cg->GenBinaryOp("||", ltmp, rtmp);
}

int LogicalExpr::GetMemBytesOr() {
    return left->GetMemBytes() + right->GetMemBytes() + CodeGenerator::VarSize;
}

Location* LogicalExpr::EmitNot(CodeGenerator *cg) {
    const char* ret_zro = cg->NewLabel();
    const char* ret_one = cg->NewLabel();
    Location *ret = cg->GenTempVar();

    Location *rtmp = right->Emit(cg);

    cg->GenIfZ(rtmp, ret_one);
    cg->GenAssign(ret, cg->GenLoadConstant(0));
    cg->GenGoto(ret_zro);
    cg->GenLabel(ret_one);
    cg->GenAssign(ret, cg->GenLoadConstant(1));
    cg->GenLabel(ret_zro);

    return ret;
}

int LogicalExpr::GetMemBytesNot() {
    return right->GetMemBytes() + 3 * CodeGenerator::VarSize;
}

Type* AssignExpr::GetType() {
    return left->GetType();
}

Location* AssignExpr::Emit(CodeGenerator *cg) {
    Location *rtemp = right->Emit(cg);
    LValue *lval = dynamic_cast<LValue*>(left);

    if (lval != NULL)
        return lval->EmitStore(cg, rtemp);

    Location *ltemp = left->Emit(cg);
    cg->GenAssign(ltemp, rtemp);
    return ltemp;
}

int AssignExpr::GetMemBytes() {
    LValue *lval = dynamic_cast<LValue*>(left);
    if (lval != NULL)
        return right->GetMemBytes() + lval->GetMemBytesStore();

    return right->GetMemBytes() + left->GetMemBytes();
}

Type* This::GetType() {
    ClassDecl *d = GetClassDecl();
    Assert(d != NULL);
    return d->GetType();
}

ArrayAccess::ArrayAccess(yyltype loc, Expr *b, Expr *s) : LValue(loc) {
    (base=b)->SetParent(this);
    (subscript=s)->SetParent(this);
}

Type* ArrayAccess::GetType() {
    return base->GetType();
}

Location* ArrayAccess::Emit(CodeGenerator *cg) {
    return cg->GenLoad(EmitAddr(cg));
}

int ArrayAccess::GetMemBytes() {
    return GetMemBytesAddr() + CodeGenerator::VarSize;
}

Location* ArrayAccess::EmitStore(CodeGenerator *cg, Location *val) {
    Location *addr = EmitAddr(cg);
    cg->GenStore(addr, val);
    return cg->GenLoad(addr);
}

int ArrayAccess::GetMemBytesStore() {
    return GetMemBytesAddr() + CodeGenerator::VarSize;
}

Location* ArrayAccess::EmitAddr(CodeGenerator *cg) {
    Location *b = base->Emit(cg);
    Location *s = subscript->Emit(cg);

    Location *con = cg->GenLoadConstant(CodeGenerator::VarSize);

    // Offset in bytes without skipping the array header info
    Location *num = cg->GenBinaryOp("*", s, con);

    // Offset in bytes taking the array header info into account
    Location *off = cg->GenBinaryOp("+", num, con);

    return cg->GenBinaryOp("+", b, off);
}

int ArrayAccess::GetMemBytesAddr() {
    return base->GetMemBytes() + subscript->GetMemBytes() +
           4 * CodeGenerator::VarSize;
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
    Assert(d != NULL);
    return d->GetType();
}

Location* FieldAccess::Emit(CodeGenerator *cg) {
    FieldAccess *baseAccess = dynamic_cast<FieldAccess*>(base);
    VarDecl *fieldDecl = GetDecl();
    Assert(fieldDecl != NULL);

    if (baseAccess == NULL)
        return fieldDecl->GetMemLoc();

    VarDecl *baseDecl = baseAccess->GetDecl();
    Assert(baseDecl != NULL);
    int fieldOffset = fieldDecl->GetMemOffset(); // TODO: Calculate actual field offset
    return cg->GenLoad(baseDecl->GetMemLoc(), fieldOffset);
}

int FieldAccess::GetMemBytes() {
    return 0;
}

Location* FieldAccess::EmitStore(CodeGenerator *cg, Location *val) {
    FieldAccess *baseAccess = dynamic_cast<FieldAccess*>(base);
    VarDecl *fieldDecl = GetDecl();
    Assert(fieldDecl != NULL);

    if (baseAccess == NULL) {
        Location *ltemp = fieldDecl->GetMemLoc();
        cg->GenAssign(ltemp, val);
        return ltemp;
    }

    VarDecl *baseDecl = baseAccess->GetDecl();
    Assert(baseDecl != NULL);
    int fieldOffset = fieldDecl->GetMemOffset(); // TODO: Calculate actual field offset
    Location *ltemp = baseDecl->GetMemLoc();
    cg->GenStore(ltemp, val, fieldOffset);
    return ltemp;
}

int FieldAccess::GetMemBytesStore() {
    return 0;
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
    Assert(d != NULL);
    return d->GetType();
}

Location* Call::Emit(CodeGenerator *cg) {
    if (IsArrayLengthCall())
        return EmitArrayLength(cg);

    return EmitLabel(cg);
}

int Call::GetMemBytes() {
    if (IsArrayLengthCall())
        return GetMemBytesArrayLength();

    return GetMemBytesLabel();
}

Location* Call::EmitLabel(CodeGenerator *cg) {
    List<Location*> *params = new List<Location*>;
    for (int i = 0, n = actuals->NumElements(); i < n; ++i)
        params->Append(actuals->Nth(i)->Emit(cg));

    int n = params->NumElements();
    for (int i = n-1; i >= 0; --i)
        cg->GenPushParam(params->Nth(i));

    Location *ret;
    if (base == NULL) {
        ret = cg->GenLCall(field->GetName(), GetDecl()->HasReturnVal());
    } else {
        Location *b = base->Emit(cg);
        Location *vtable = cg->GenLoad(b);
        int methodOffset = 0; // TODO: Calculate actual method offset
        Location *faddr = cg->GenLoad(vtable, methodOffset);
        ret = cg->GenACall(faddr, GetDecl()->HasReturnVal());
    }

    // TODO: Support variable Object sizes
    cg->GenPopParams(n * CodeGenerator::VarSize);

    return ret;
}

int Call::GetMemBytesLabel() {
    int memBytes = 0;
    for (int i = 0, n = actuals->NumElements(); i < n; ++i)
        memBytes += actuals->Nth(i)->GetMemBytes();

    if (base != NULL)
        memBytes += base->GetMemBytes() + 2 * CodeGenerator::VarSize;

    if (GetDecl()->HasReturnVal())
        memBytes += CodeGenerator::VarSize;

    return memBytes;
}

Location* Call::EmitArrayLength(CodeGenerator *cg) {
    return cg->GenLoad(base->Emit(cg));
}

int Call::GetMemBytesArrayLength() {
    return base->GetMemBytes() + CodeGenerator::VarSize;
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
    Assert(c != NULL);
    return c->GetType();
}

Location* NewExpr::Emit(CodeGenerator *cg) {
    const char *name = cType->GetName();

    Decl *d = Program::gScope->table->Lookup(name);
    Assert(d != NULL);

    Location *s = cg->GenLoadConstant(d->GetMemBytes());
    Location *c = cg->GenLoadConstant(CodeGenerator::VarSize);

    Location *mem = cg->GenBuiltInCall(Alloc, cg->GenBinaryOp("+", c, s));
    cg->GenStore(mem, cg->GenLoadLabel(name));

    return mem;
}

int NewExpr::GetMemBytes() {
    return 5 * CodeGenerator::VarSize;
}

NewArrayExpr::NewArrayExpr(yyltype loc, Expr *sz, Type *et) : Expr(loc) {
    Assert(sz != NULL && et != NULL);
    (size=sz)->SetParent(this);
    (elemType=et)->SetParent(this);
}

Type* NewArrayExpr::GetType() {
    return new ArrayType(elemType);
}

Location* NewArrayExpr::Emit(CodeGenerator *cg) {
    Location *s = size->Emit(cg);
    Location *c = cg->GenLoadConstant(CodeGenerator::VarSize);

    Location *n = cg->GenBinaryOp("*", s, c);
    Location *mem = cg->GenBuiltInCall(Alloc, cg->GenBinaryOp("+", c, n));
    cg->GenStore(mem, s);

    return mem;
}

int NewArrayExpr::GetMemBytes() {
    return size->GetMemBytes() + 4 * CodeGenerator::VarSize;
}

Type* ReadIntegerExpr::GetType() {
    return Type::intType;
}

Location* ReadIntegerExpr::Emit(CodeGenerator *cg) {
    return cg->GenBuiltInCall(ReadInteger);
}

int ReadIntegerExpr::GetMemBytes() {
    return CodeGenerator::VarSize;
}

Type* ReadLineExpr::GetType() {
    return Type::stringType;
}

Location* ReadLineExpr::Emit(CodeGenerator *cg) {
    return cg->GenBuiltInCall(ReadLine);
}

int ReadLineExpr::GetMemBytes() {
    return CodeGenerator::VarSize;
}
