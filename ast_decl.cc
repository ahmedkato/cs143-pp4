/* File: ast_decl.cc
 * -----------------
 * Implementation of Decl node classes.
 */
#include "ast_decl.h"
#include "ast_type.h"
#include "ast_stmt.h"
#include "codegen.h"

Decl::Decl(Identifier *n) : Node(*n->GetLocation()) {
    Assert(n != NULL);
    (id=n)->SetParent(this);
    scope = new Scope;
}

VarDecl::VarDecl(Identifier *n, Type *t) : Decl(n) {
    Assert(n != NULL && t != NULL);
    (type=t)->SetParent(this);
}

int VarDecl::GetMemBytes() {
    return CodeGenerator::VarSize;
}

ClassDecl::ClassDecl(Identifier *n, NamedType *ex, List<NamedType*> *imp, List<Decl*> *m) : Decl(n) {
    // extends can be NULL, impl & mem may be empty lists but cannot be NULL
    Assert(n != NULL && imp != NULL && m != NULL);
    extends = ex;
    if (extends) extends->SetParent(this);
    (implements=imp)->SetParentAll(this);
    (members=m)->SetParentAll(this);

}

NamedType* ClassDecl::GetType() {
    return new NamedType(id);
}

void ClassDecl::BuildScope() {
    for (int i = 0, n = members->NumElements(); i < n; ++i)
        scope->AddDecl(members->Nth(i));

    for (int i = 0, n = members->NumElements(); i < n; ++i)
        members->Nth(i)->BuildScope();
}

void ClassDecl::PreEmit() {
    int memOffset = CodeGenerator::OffsetToFirstField;
    int vtblOffset = CodeGenerator::OffsetToFirstMethod;

    if (extends != NULL) {
        Decl *d = Program::gScope->table->Lookup(extends->GetName());
        Assert(d != NULL);
        memOffset += d->GetMemBytes();
        vtblOffset += d->GetVTblBytes();
    }

    for (int i = 0, n = members->NumElements(); i < n; ++i) {
        VarDecl *d = dynamic_cast<VarDecl*>(members->Nth(i));
        if (d == NULL)
            continue;
        d->SetMemOffset(memOffset);
        memOffset += d->GetMemBytes();

    }

    for (int i = 0, n = members->NumElements(); i < n; ++i) {
        FnDecl *d = dynamic_cast<FnDecl*>(members->Nth(i));
        if (d == NULL)
            continue;
        d->SetIsMethod(true);
        d->SetVTblOffset(vtblOffset);
        vtblOffset += CodeGenerator::VarSize;
    }

    for (int i = 0, n = members->NumElements(); i < n; ++i) {
        std::string prefix;
        prefix += GetName();
        prefix += ".";
        members->Nth(i)->AddLabelPrefix(prefix.c_str());
    }
}

Location* ClassDecl::Emit(CodeGenerator *cg) {
    for (int i = 0, n = members->NumElements(); i < n; ++i)
        members->Nth(i)->Emit(cg);

    List<FnDecl*> *decls = GetMethodDecls();
    List<const char*> *labels = new List<const char*>;
    for (int i = 0, n = decls->NumElements(); i < n; ++i)
        labels->Append(decls->Nth(i)->GetLabel());

    cg->GenVTable(GetName(), labels);

    return NULL;
}

int ClassDecl::GetMemBytes() {
    int memBytes = 0;

    if (extends != NULL) {
        Decl *d = Program::gScope->table->Lookup(extends->GetName());
        Assert(d != NULL);
        memBytes += d->GetMemBytes();
    }

    for (int i = 0, n = members->NumElements(); i < n; ++i)
        memBytes += members->Nth(i)->GetMemBytes();

    return memBytes;
}

int ClassDecl::GetVTblBytes() {
    int vtblBytes = 0;

    if (extends != NULL) {
        Decl *d = Program::gScope->table->Lookup(extends->GetName());
        Assert(d != NULL);
        vtblBytes += d->GetVTblBytes();
    }

    for (int i = 0, n = members->NumElements(); i < n; ++i)
        vtblBytes += members->Nth(i)->GetVTblBytes();

    return vtblBytes;
}

List<FnDecl*>* ClassDecl::GetMethodDecls() {
    List<FnDecl*> *decls = new List<FnDecl*>;

    if (extends != NULL) {
        Decl *d = Program::gScope->table->Lookup(extends->GetName());
        ClassDecl *c = dynamic_cast<ClassDecl*>(d);
        Assert(c != NULL);
        List<FnDecl*> *extDecls = c->GetMethodDecls();
        for (int i = 0, n = extDecls->NumElements(); i < n; ++i)
            decls->Append(extDecls->Nth(i));
    }

    for (int i = 0, n = members->NumElements(); i < n; ++i) {
        FnDecl *d = dynamic_cast<FnDecl*>(members->Nth(i));
        if (d == NULL)
            continue;

        for (int j = 0, m = decls->NumElements(); j < m; ++j) {
            if (strcmp(decls->Nth(j)->GetName(), d->GetName()) == 0) {
                decls->RemoveAt(j);
                decls->InsertAt(d, j);
            }
        }
    }

    for (int i = 0, n = members->NumElements(); i < n; ++i) {
        FnDecl *d = dynamic_cast<FnDecl*>(members->Nth(i));
        if (d == NULL)
            continue;
        decls->Append(d);
    }

    return decls;
}

InterfaceDecl::InterfaceDecl(Identifier *n, List<Decl*> *m) : Decl(n) {
    Assert(n != NULL && m != NULL);
    (members=m)->SetParentAll(this);
}

void InterfaceDecl::BuildScope() {
    for (int i = 0, n = members->NumElements(); i < n; ++i)
        scope->AddDecl(members->Nth(i));

    for (int i = 0, n = members->NumElements(); i < n; ++i)
        members->Nth(i)->BuildScope();
}

FnDecl::FnDecl(Identifier *n, Type *r, List<VarDecl*> *d) : Decl(n) {
    Assert(n != NULL && r!= NULL && d != NULL);
    (returnType=r)->SetParent(this);
    (formals=d)->SetParentAll(this);
    body = NULL;
    label = new std::string(GetName());
    isMethod = false;
}

void FnDecl::SetFunctionBody(Stmt *b) {
    (body=b)->SetParent(this);
}

const char* FnDecl::GetLabel() {
    return label->c_str();
}

bool FnDecl::HasReturnVal() {
    return returnType == Type::voidType ? 0 : 1;
}

void FnDecl::BuildScope() {
    for (int i = 0, n = formals->NumElements(); i < n; ++i)
        scope->AddDecl(formals->Nth(i));

    for (int i = 0, n = formals->NumElements(); i < n; ++i)
        formals->Nth(i)->BuildScope();

    if (body) body->BuildScope();
}

Location* FnDecl::Emit(CodeGenerator *cg) {
    int offset = CodeGenerator::OffsetToFirstParam;

    if (isMethod)
        offset += CodeGenerator::VarSize;

    for (int i = 0, n = formals->NumElements(); i < n; ++i) {
        VarDecl *d = formals->Nth(i);
        Location *loc = new Location(fpRelative, offset, d->GetName());
        d->SetMemLoc(loc);
        offset += d->GetMemBytes();
    }

    if (body != NULL) {
        cg->GenLabel(GetLabel());
        cg->GenBeginFunc()->SetFrameSize(body->GetMemBytes());
        body->Emit(cg);
        cg->GenEndFunc();
    }

    return NULL;
}

int FnDecl::GetVTblBytes() {
    return CodeGenerator::VarSize;
}

void FnDecl::AddLabelPrefix(const char *p) {
    label->insert(0, p);
}
