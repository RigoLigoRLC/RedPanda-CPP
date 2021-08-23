#include "classbrowser.h"
#include "../utils.h"

ClassBrowserModel::ClassBrowserModel(QObject *parent):QAbstractItemModel(parent)
{
    mRoot = new ClassBrowserNode();
    mRoot->parent = nullptr;
    mRoot->statement = PStatement();
    mRoot->childrenFetched = true;
    mUpdating = false;
    mUpdateCount = 0;
    mShowInheritedMembers = false;
}

ClassBrowserModel::~ClassBrowserModel()
{
    delete mRoot;
}

QModelIndex ClassBrowserModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row,column,parent))
        return QModelIndex();

    ClassBrowserNode *parentNode;
    if (!parent.isValid()) { // top level
        parentNode = mRoot;
    } else {
        parentNode = static_cast<ClassBrowserNode *>(parent.internalPointer());
    }
    return createIndex(row,column,parentNode->children[row]);
}

QModelIndex ClassBrowserModel::parent(const QModelIndex &child) const
{
    if (child.isValid()) {
        return QModelIndex();
    }
    ClassBrowserNode *childNode = static_cast<ClassBrowserNode *>(child.internalPointer());
    ClassBrowserNode *parentNode = childNode->parent;
    if (parentNode->parent == nullptr) //it's root node
        return QModelIndex();

    ClassBrowserNode *grandNode = parentNode->parent;
    int row = grandNode->children.indexOf(parentNode);
    return createIndex(row,0,parentNode);
}

int ClassBrowserModel::rowCount(const QModelIndex &parent) const
{
    ClassBrowserNode *parentNode;
    if (!parent.isValid()) { // top level
        parentNode = mRoot;
    } else {
        parentNode = static_cast<ClassBrowserNode *>(parent.internalPointer());
    }
    return parentNode->children.count();
}

int ClassBrowserModel::columnCount(const QModelIndex&) const
{
    return 1;
}

void ClassBrowserModel::fetchMore(const QModelIndex &parent)
{
    if (!parent.isValid()) { // top level
        return;
    }

    ClassBrowserNode *parentNode = static_cast<ClassBrowserNode *>(parent.internalPointer());
    if (!parentNode->childrenFetched) {
        parentNode->childrenFetched = true;
        if (parentNode->statement && !parentNode->statement->children.isEmpty())
            filterChildren(parentNode, parentNode->statement->children);
    }
}

bool ClassBrowserModel::canFetchMore(const QModelIndex &parent) const
{

    if (!parent.isValid()) { // top level
        return false;
    }

    ClassBrowserNode *parentNode = static_cast<ClassBrowserNode *>(parent.internalPointer());
    if (!parentNode->childrenFetched) {
        if (parentNode->statement && !parentNode->statement->children.isEmpty())
            return true;
        else
            parentNode->childrenFetched = true;
    }
    return false;
}

QVariant ClassBrowserModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()){
        return QVariant();
    }
    ClassBrowserNode *node = static_cast<ClassBrowserNode *>(index.internalPointer());
    if (!node)
        return QVariant();
    if (role == Qt::DisplayRole) {
        if (node->statement) {
            return node->statement->command;
        }
    }
    return QVariant();
}

const PCppParser &ClassBrowserModel::cppParser() const
{
    return mParser;
}

void ClassBrowserModel::setCppParser(const PCppParser &newCppParser)
{
    if (mParser) {
        disconnect(mParser.get(),
                   &CppParser::onEndParsing,
                   this,
                   &ClassBrowserModel::fillStatements);
    }
    mParser = newCppParser;
    if (mParser) {
        connect(mParser.get(),
                   &CppParser::onEndParsing,
                   this,
                   &ClassBrowserModel::fillStatements);
        if (!mParser->parsing())
            fillStatements();
    }
}

void ClassBrowserModel::clear()
{
    beginResetModel();
    mRoot->children.clear();
    mNodes.clear();
    mDummyStatements.clear();
    endResetModel();
}

void ClassBrowserModel::fillStatements()
{
    QMutexLocker locker(&mMutex);
    if (mUpdateCount!=0)
        return;
    mUpdating = true;
    beginResetModel();
    clear();
    {
        auto action = finally([this]{
            endResetModel();
            mUpdating = false;
        });
        if (!mParser)
            return;
        if (!mParser->enabled())
            return;
        if (!mParser->freeze())
            return;
        {
            auto action2 = finally([this]{
                mParser->unFreeze();
            });
            QString mParserSerialId = mParser->serialId();
            if (!mCurrentFile.isEmpty()) {
                QSet<QString> includedFiles = mParser->getFileIncludes(mCurrentFile);

                addMembers(includedFiles);
                // Remember selection
//                  if fLastSelection <> '' then
//                    ReSelect;
            }

        }
    }
}

void ClassBrowserModel::addChild(ClassBrowserNode *node, PStatement statement)
{
    PClassBrowserNode newNode = std::make_shared<ClassBrowserNode>();
    newNode->parent = node;
    newNode->statement = statement;
    newNode->childrenFetched = false;
    node->children.append(newNode.get());
    mNodes.append(newNode);
}

void ClassBrowserModel::addMembers(const QSet<QString> &includedFiles)
{
    // show statements in the file
    PFileIncludes p = mParser->findFileIncludes(mCurrentFile);
    if (!p)
        return;
    filterChildren(mRoot,p->statements);
}

void ClassBrowserModel::filterChildren(ClassBrowserNode *node, const StatementMap &statements)
{
    for (PStatement statement:statements) {
        if (statement->kind == StatementKind::skBlock)
            continue;
        if (statement->isInherited && !mShowInheritedMembers)
            continue;

        if (statement == node->statement) // prevent infinite recursion
            continue;

        if (statement->scope == StatementScope::ssLocal)
            continue;


//        if (fStatementsType = cbstProject) then begin
//          if not Statement^._InProject then
//            Continue;
//          if Statement^._Static and not SameText(Statement^._FileName,fCurrentFile)
//            and not SameText(Statement^._FileName,fCurrentFile) then
//            Continue;
//        end;

        // we only test and handle orphan statements in the top level (node->statement is null)
        PStatement parentScope = statement->parentScope.lock();
        if ((parentScope!=node->statement) && (!node->statement)) {

//          // we only handle orphan statements when type is cbstFile
//          if fStatementsType <> cbstFile then
//            Continue;

//          //should not happend, just in case of error
            if (!parentScope)
                continue;

            // Processing the orphan statement
            while (statement) {
                //the statement's parent is in this file, so it's not a real orphan
                if ((parentScope->fileName==mCurrentFile)
                        ||(parentScope->definitionFileName==mCurrentFile))
                    break;

                PStatement dummyParent = mDummyStatements.value(parentScope->fullName,PStatement());
                if (dummyParent) {
                    dummyParent->children.insert(statement->command,statement);
                    break;
                }
                dummyParent = createDummy(parentScope);
                dummyParent->children.insert(statement->command,statement);
                //we are adding an orphan statement, just add it
                statement = dummyParent;
                parentScope = statement->parentScope.lock();
                if (!parentScope) {
                    addChild(node,statement);

                    break;
                }
            }
        } else if (statement->kind == StatementKind::skNamespace) {
            PStatement dummy = mDummyStatements.value(statement->fullName,PStatement());
            if (dummy) {
                for (PStatement child: statement->children) {
                    dummy->children.insert(child->command,child);
                }
                continue;
            }
            dummy = createDummy(statement);
            dummy->children = statement->children;
            addChild(node,dummy);
        } else {
            addChild(node,statement);
        }
    }
//    if sortAlphabetically and sortByType then begin
//      filtered.Sort(@CompareByAlphaAndType);
//    end else if sortAlphabetically then begin
//      filtered.Sort(@CompareByAlpha);
//    end else if sortByType then begin
//      filtered.Sort(@CompareByType);
    //    end;
}

PStatement ClassBrowserModel::createDummy(PStatement statement)
{
    PStatement result = std::make_shared<Statement>();
    result->parentScope = statement->parentScope;
    result->command = statement->command;
    result->args = statement->args;
    result->noNameArgs = statement->noNameArgs;
    result->fullName = statement->fullName;
    result->kind = statement->kind;
    result->type = statement->type;
    result->value = statement->value;
    result->scope = statement->scope;
    result->classScope = statement->classScope;
    result->inProject = statement->inProject;
    result->inSystemHeader = statement->inSystemHeader;
    result->isStatic = statement->isStatic;
    result->isInherited = statement->isInherited;
    result->fileName = mCurrentFile;
    result->definitionFileName = mCurrentFile;
    result->line = 0;
    result->definitionLine = 0;
    mDummyStatements.insert(result->fullName,result);
    return result;
}

