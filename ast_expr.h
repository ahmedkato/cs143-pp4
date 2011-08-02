/* File: ast_expr.h
 * ----------------
 * The Expr class and its subclasses are used to represent
 * expressions in the parse tree.  For each expression in the
 * language (add, call, New, etc.) there is a corresponding
 * node class for that construct.
 *
 * pp4: You will need to extend the Expr classes to implement
 * code generation for expressions.
 */

#ifndef _H_ast_expr
#define _H_ast_expr

#include "ast.h"
#include "ast_stmt.h"
#include "list.h"

class NamedType; // for new
class Type; // for NewArray
class CodeGenerator;
class ClassDecl;

class Expr : public Stmt
{
  public:
    Expr(yyltype loc) : Stmt(loc) {}
    Expr() : Stmt() {}

    // TODO: Make into a pure virtual function
    virtual Type* GetType() { return NULL; }
    void BuildScope() { /* Empty */ }

  protected:
    Decl* GetFieldDecl(Identifier *field, Node *n);
    Decl* GetFieldDecl(Identifier *field, Type *t);
    ClassDecl* GetClassDecl();
};

/* This node type is used for those places where an expression is optional.
 * We could use a NULL pointer, but then it adds a lot of checking for
 * NULL. By using a valid, but no-op, node, we save that trouble */
class EmptyExpr : public Expr
{
  public:
};

class IntConstant : public Expr
{
  protected:
    int value;

  public:
    IntConstant(yyltype loc, int val);

    Type* GetType();
    Location* Emit(CodeGenerator *cg);
};

class DoubleConstant : public Expr
{
  protected:
    double value;

  public:
    DoubleConstant(yyltype loc, double val);

    Type* GetType();
    Location* Emit(CodeGenerator *cg);
};

class BoolConstant : public Expr
{
  protected:
    bool value;

  public:
    BoolConstant(yyltype loc, bool val);

    Type* GetType();
    Location* Emit(CodeGenerator *cg);
};

class StringConstant : public Expr
{
  protected:
    char *value;

  public:
    StringConstant(yyltype loc, const char *val);

    Type* GetType();
    Location* Emit(CodeGenerator *cg);
};

class NullConstant: public Expr
{
  public:
    NullConstant(yyltype loc) : Expr(loc) {}

    Type* GetType();
    Location* Emit(CodeGenerator *cg);
};

class Operator : public Node
{
  protected:
    char tokenString[4];

  public:
    Operator(yyltype loc, const char *tok);
    friend ostream& operator<<(ostream& out, Operator *o) { return out << o->tokenString; }

    const char *GetTokenString() { return tokenString; }
};

class CompoundExpr : public Expr
{
  protected:
    Operator *op;
    Expr *left, *right; // left will be NULL if unary

  public:
    CompoundExpr(Expr *lhs, Operator *op, Expr *rhs); // for binary
    CompoundExpr(Operator *op, Expr *rhs);             // for unary

    virtual Location* Emit(CodeGenerator *cg) = 0;
};

class ArithmeticExpr : public CompoundExpr
{
  public:
    ArithmeticExpr(Expr *lhs, Operator *op, Expr *rhs) : CompoundExpr(lhs,op,rhs) {}
    ArithmeticExpr(Operator *op, Expr *rhs) : CompoundExpr(op,rhs) {}

    Location* Emit(CodeGenerator *cg);
};

class RelationalExpr : public CompoundExpr
{
  public:
    RelationalExpr(Expr *lhs, Operator *op, Expr *rhs) : CompoundExpr(lhs,op,rhs) {}

    Location* Emit(CodeGenerator *cg);
};

class EqualityExpr : public CompoundExpr
{
  public:
    EqualityExpr(Expr *lhs, Operator *op, Expr *rhs) : CompoundExpr(lhs,op,rhs) {}
    const char *GetPrintNameForNode() { return "EqualityExpr"; }

    Location* Emit(CodeGenerator *cg);
};

class LogicalExpr : public CompoundExpr
{
  public:
    LogicalExpr(Expr *lhs, Operator *op, Expr *rhs) : CompoundExpr(lhs,op,rhs) {}
    LogicalExpr(Operator *op, Expr *rhs) : CompoundExpr(op,rhs) {}
    const char *GetPrintNameForNode() { return "LogicalExpr"; }

    Location* Emit(CodeGenerator *cg);
};

class AssignExpr : public CompoundExpr
{
  public:
    AssignExpr(Expr *lhs, Operator *op, Expr *rhs) : CompoundExpr(lhs,op,rhs) {}
    const char *GetPrintNameForNode() { return "AssignExpr"; }

    Location* Emit(CodeGenerator *cg);
};

class LValue : public Expr
{
  public:
    LValue(yyltype loc) : Expr(loc) {}

    // TODO: Make into a pure virtual function
    virtual Location* Emit(CodeGenerator *cg) { return NULL; }
};

class This : public Expr
{
  public:
    This(yyltype loc) : Expr(loc) {}
};

class ArrayAccess : public LValue
{
  protected:
    Expr *base, *subscript;

  public:
    ArrayAccess(yyltype loc, Expr *base, Expr *subscript);
};

/* Note that field access is used both for qualified names
 * base.field and just field without qualification. We don't
 * know for sure whether there is an implicit "this." in
 * front until later on, so we use one node type for either
 * and sort it out later. */
class FieldAccess : public LValue
{
  protected:
    Expr *base;	// will be NULL if no explicit base
    Identifier *field;

  public:
    FieldAccess(Expr *base, Identifier *field); //ok to pass NULL base

    Type* GetType();
    Location* Emit(CodeGenerator *cg);

  private:
    VarDecl* GetDecl();
};

/* Like field access, call is used both for qualified base.field()
 * and unqualified field().  We won't figure out until later
 * whether we need implicit "this." so we use one node type for either
 * and sort it out later. */
class Call : public Expr
{
  protected:
    Expr *base;	// will be NULL if no explicit base
    Identifier *field;
    List<Expr*> *actuals;

  public:
    Call(yyltype loc, Expr *base, Identifier *field, List<Expr*> *args);
};

class NewExpr : public Expr
{
  protected:
    NamedType *cType;

  public:
    NewExpr(yyltype loc, NamedType *clsType);
};

class NewArrayExpr : public Expr
{
  protected:
    Expr *size;
    Type *elemType;

  public:
    NewArrayExpr(yyltype loc, Expr *sizeExpr, Type *elemType);
};

class ReadIntegerExpr : public Expr
{
  public:
    ReadIntegerExpr(yyltype loc) : Expr(loc) {}
};

class ReadLineExpr : public Expr
{
  public:
    ReadLineExpr(yyltype loc) : Expr (loc) {}
};

#endif
