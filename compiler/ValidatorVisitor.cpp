#include "ValidatorVisitor.h"

ValidatorVisitor::ValidatorVisitor(){
    definedFunctions = new vector<tuple<Type,string>>();
    declaredVariables_list = new vector<vector<map<string, tuple<int, int>>*>*>();
    declaredVariables = new vector<map<string, tuple<int, int>>*>();
    declaredVariables->push_back(new map<string, tuple<int, int>>());
    declaredVariables_list->push_back(declaredVariables);
}

bool ValidatorVisitor::callingVoidFunctionInChildren(antlr4::ParserRuleContext * ctx) {
    for(auto child : ctx->children) {
        if(auto expr = dynamic_cast<ifccParser::ExprCALLContext*>(child)) {
            string functionName = expr->ID()->getText();
            for (tuple<Type,string> definedFunction : *definedFunctions) {
                if (get<1>(definedFunction) == functionName && get<0>(definedFunction) == VOID)
                    return true;
            }
        }
    }
    return false;
}

antlrcpp::Any ValidatorVisitor::visitAffectation(ifccParser::AffectationContext *ctx) {
    if(callingVoidFunctionInChildren(ctx)) {
        cerr << "Void function called in expression\n";
        exit(1);
    }
    string nom = ctx->ID()->getText();

    if (findVariable(nom) == nullptr) {
        cerr << "Variable " << nom << " utilisée sans être déclarée\n";
        exit(2); // variable non déclarée
    } else {
        get<0>(*findVariable(nom)) = max(1, get<0>(*findVariable(nom)));
    }

    visit(ctx->expression());

    return 0;
}

antlrcpp::Any ValidatorVisitor::visitDeclaration(ifccParser::DeclarationContext *ctx)
{
    if(callingVoidFunctionInChildren(ctx)) {
        cerr << "Void function called in expression\n";
        exit(1);
    }
    string nom = ctx->ID()->getText();

    if (declaredVariables->back()->find(nom) != declaredVariables->back()->end()) {
        cerr << "Redéfinition de la variable " << nom << "\n";
        exit (1); // variable déjà déclarée dans le contexte actuel
    }

    if(declaredVariables->size() == 2 && declaredVariables->front()->find(nom) != declaredVariables->front()->end() )  {
            cerr << "Nom de variable existe déjà comme paramètre\n";
            exit(1);
    }

    if (ctx->expression() == nullptr) {
        declaredVariables->back()->insert(make_pair(nom, tuple(0, (declaredVariables->size() + 1) * 4)));
        return 0;
    }

    declaredVariables->back()->insert(make_pair(nom, tuple(0, (declaredVariables->size() + 1) * 4)));

    visit(ctx->expression());
    get<0>(*findVariable(nom)) = 1;

    return 0;
}

antlrcpp::Any ValidatorVisitor::visitValeur(ifccParser::ValeurContext *ctx) {
    if (ctx->ID() == nullptr) {
        return 0;
    }

    string nom = ctx->ID()->getText();

    if (findVariable(nom) == nullptr) {
        cerr << "Variable " << nom << " utilisée sans être déclarée\n";
        exit(2); // variable non déclarée
    } else if (get<0>(*findVariable(nom)) == 0) {
        cerr << "Variable " << nom << " utilisée sans être initialisée\n";
    }

    get<0>(*findVariable(nom)) = 2;
    return 0;
}

antlrcpp::Any ValidatorVisitor::visitBloc(ifccParser::BlocContext *ctx) {
    declaredVariables->push_back(new map<string, tuple<int, int>>());
    visitChildren(ctx);
    for (auto & variable : *(declaredVariables->back())) {
        if (get<0>(variable.second) < 2) {
            cerr << "Variable " << variable.first << " inutilisée\n";
        }
    }
    declaredVariables->pop_back();
    return 0;
}

tuple<int,int>* ValidatorVisitor::findVariable(string nom) {
    for (auto blocVariable = declaredVariables->rbegin(); blocVariable != declaredVariables->rend(); blocVariable++) {
        for (auto & [nomVariable,informations] : **blocVariable) {
            if (nomVariable == nom) {
                return &informations;
            }
        }
    }
    return nullptr;
}

antlrcpp::Any ValidatorVisitor::visitControl_flow_instruction(ifccParser::Control_flow_instructionContext *ctx) {
    antlr4::tree::ParseTree* parent = ctx->parent;
    while(
            dynamic_cast<ifccParser::While_loopContext *>(parent) == nullptr &&
            dynamic_cast<ifccParser::For_loopContext *>(parent) == nullptr &&
            dynamic_cast<ifccParser::Do_while_loopContext *>(parent) == nullptr
    ) {
        if (parent == nullptr) {
            cerr << "Instruction " << ctx->getText() << " utilisée dans un contexte invalide\n";
            exit(3);
        }
        parent = parent->parent;
    }
    return 0;
}

antlrcpp::Any ValidatorVisitor::visitProg(ifccParser::ProgContext *ctx){
    visitChildren(ctx);
    for (tuple<Type,string> definedFunction : *definedFunctions) {
        if (get<1>(definedFunction) == "main") {
            return 0;
        }
    }
    cerr << "Fonction main not defined\n";
    exit(5);
}

antlrcpp::Any ValidatorVisitor::visitFunction(ifccParser::FunctionContext *ctx){
    string functionName = ctx->ID()->getText();
    for (tuple<Type,string> definedFunction : *definedFunctions) {
        if (get<1>(definedFunction) == functionName) {
            cerr << "Fonction " << functionName << " already defined\n";
            exit(4);
        }
    }

    Type functionType = ctx->type()->getText() == "int"? Type::INT : Type::VOID;

    declaredVariables = new vector<map<string, tuple<int, int>>*>();
    declaredVariables->push_back(new map<string, tuple<int, int>>());
    declaredVariables_list->push_back(declaredVariables);
    definedFunctions->push_back(std::make_tuple(functionType,functionName));

    visitChildren(ctx);

    return 0;
}

antlrcpp::Any ValidatorVisitor::visitParam(ifccParser::ParamContext *context) {
    string nom = context->ID()->getText();

    if(declaredVariables->back()->find(nom) != declaredVariables->back()->end() )  {
            cerr << "Parameter defined more than once\n";
            exit(1);
    }
    declaredVariables->back()->insert(make_pair(nom, tuple(0, (declaredVariables->size() + 1) * 4)));
    get<0>(*findVariable(nom)) = 1;

    return 0;
}

antlrcpp::Any ValidatorVisitor::visitExprCALL(ifccParser::ExprCALLContext *context) {
    if(callingVoidFunctionInChildren(context)) {
        cerr << "Void function called in expression\n";
        exit(1);
    }
    string nom = context->ID()->getText();
    if(findVariable(nom) != nullptr) {
        cerr << "function name same as local var\n";
        exit(1);
    }
    return 0;
}

antlrcpp::Any ValidatorVisitor::visitFor_loop(ifccParser::For_loopContext *ctx) {
    declaredVariables->push_back(new map<string, tuple<int, int>>());
    visitChildren(ctx);
    for (auto & variable : *(declaredVariables->back())) {
        if (get<0>(variable.second) < 2) {
            cerr << "Variable " << variable.first << " inutilisée\n";
        }
    }
    declaredVariables->pop_back();
    return 0;
}

antlrcpp::Any ValidatorVisitor::visitReturn_stmt(ifccParser::Return_stmtContext *context)
{
    if(callingVoidFunctionInChildren(context)) {
        cerr << "Void function called in expression\n";
        exit(1);
    }
    visitChildren(context);
    return 0;
}
antlrcpp::Any ValidatorVisitor::visitIfelse(ifccParser::IfelseContext *context)
{
    if(callingVoidFunctionInChildren(context)) {
        cerr << "Void function called in expression\n";
        exit(1);
    }
    visitChildren(context);
    return 0;
}
antlrcpp::Any ValidatorVisitor::visitWhile_loop(ifccParser::While_loopContext *context)
{
    if(callingVoidFunctionInChildren(context)) {
        cerr << "Void function called in expression\n";
        exit(1);
    }
    visitChildren(context);
    return 0;
}
antlrcpp::Any ValidatorVisitor::visitExprLOR(ifccParser::ExprLORContext *context)
{
    if(callingVoidFunctionInChildren(context)) {
        cerr << "Void function called in expression\n";
        exit(1);
    }
    visitChildren(context);
    return 0;
}
antlrcpp::Any ValidatorVisitor::visitExprUNAIRE(ifccParser::ExprUNAIREContext *context)
{
    if(callingVoidFunctionInChildren(context)) {
        cerr << "Void function called in expression\n";
        exit(1);
    }
    visitChildren(context);
    return 0;
}
antlrcpp::Any ValidatorVisitor::visitExprNE(ifccParser::ExprNEContext *context)
{
    if(callingVoidFunctionInChildren(context)) {
        cerr << "Void function called in expression\n";
        exit(1);
    }
    visitChildren(context);
    return 0;
}
antlrcpp::Any ValidatorVisitor::visitExprEQ(ifccParser::ExprEQContext *context)
{
    if(callingVoidFunctionInChildren(context)) {
        cerr << "Void function called in expression\n";
        exit(1);
    }
    visitChildren(context);
    return 0;
}
antlrcpp::Any ValidatorVisitor::visitExprLAND(ifccParser::ExprLANDContext *context)
{
    if(callingVoidFunctionInChildren(context)) {
        cerr << "Void function called in expression\n";
        exit(1);
    }
    visitChildren(context);
    return 0;
}
antlrcpp::Any ValidatorVisitor::visitExprAS(ifccParser::ExprASContext *context)
{
    if(callingVoidFunctionInChildren(context)) {
        cerr << "Void function called in expression\n";
        exit(1);
    }
    visitChildren(context);
    return 0;
}
antlrcpp::Any ValidatorVisitor::visitExprOR(ifccParser::ExprORContext *context)
{
    if(callingVoidFunctionInChildren(context)) {
        cerr << "Void function called in expression\n";
        exit(1);
    }
    visitChildren(context);
    return 0;
}
antlrcpp::Any ValidatorVisitor::visitExprAND(ifccParser::ExprANDContext *context)
{
    if(callingVoidFunctionInChildren(context)) {
        cerr << "Void function called in expression\n";
        exit(1);
    }
    visitChildren(context);
    return 0;
}
antlrcpp::Any ValidatorVisitor::visitExprBWSHIFT(ifccParser::ExprBWSHIFTContext *context)
{
    if(callingVoidFunctionInChildren(context)) {
        cerr << "Void function called in expression\n";
        exit(1);
    }
    visitChildren(context);
    return 0;
}
antlrcpp::Any ValidatorVisitor::visitExprMDM(ifccParser::ExprMDMContext *context)
{
    if(callingVoidFunctionInChildren(context)) {
        cerr << "Void function called in expression\n";
        exit(1);
    }
    visitChildren(context);
    return 0;
}
antlrcpp::Any ValidatorVisitor::visitExprXOR(ifccParser::ExprXORContext *context)
{
    if(callingVoidFunctionInChildren(context)) {
        cerr << "Void function called in expression\n";
        exit(1);
    }
    visitChildren(context);
    return 0;
}
