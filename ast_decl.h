/* File: ast_decl.h
 * ----------------
 * In our parse tree, Decl nodes are used to represent and
 * manage declarations. There are 4 subclasses of the base class,
 * specialized for declarations of variables, functions, classes,
 * and interfaces.
 *
 * pp4: You will need to extend the Decl classes to implement
 * code generation for declarations.
 */

#ifndef _H_ast_decl
#define _H_ast_decl

#include "ast.h"
#include "list.h"

class Type;
class NamedType;
class Identifier;
class Stmt;
class CodeGenerator;
class Location;
class Scope;

class Decl : public Node
{
  protected:
    Identifier *id;
    Scope *scope;

  public:
    Decl(Identifier *name);
    friend ostream& operator<<(ostream& out, Decl *d) { return out << d->id; }

    const char* GetName() { return id->GetName(); }

    // TODO: Make into pure virtual functions
    virtual void BuildScope() {}
    virtual Location* Emit(CodeGenerator *cg) { return NULL; }
};

class VarDecl : public Decl
{
  protected:
    Type *type;

  public:
    VarDecl(Identifier *name, Type *type);
};

class ClassDecl : public Decl
{
  protected:
    List<Decl*> *members;
    NamedType *extends;
    List<NamedType*> *implements;

  public:
    ClassDecl(Identifier *name, NamedType *extends,
              List<NamedType*> *implements, List<Decl*> *members);
};

class InterfaceDecl : public Decl
{
  protected:
    List<Decl*> *members;

  public:
    InterfaceDecl(Identifier *name, List<Decl*> *members);
};

class FnDecl : public Decl
{
  protected:
    List<VarDecl*> *formals;
    Type *returnType;
    Stmt *body;

  public:
    FnDecl(Identifier *name, Type *returnType, List<VarDecl*> *formals);
    void SetFunctionBody(Stmt *b);

    void BuildScope();
    Location* Emit(CodeGenerator *cg);
};

#endif
