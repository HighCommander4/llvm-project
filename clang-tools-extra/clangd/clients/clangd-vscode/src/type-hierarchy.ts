import * as vscode from 'vscode';
import * as vscodelc from 'vscode-languageclient';

export namespace TypeHierarchyDirection {
export const Children = 0;
export const Parents = 1;
export const Both = 2;
}

type TypeHierarchyDirection = 0|1|2;

interface TypeHierarchyParams extends vscodelc.TextDocumentPositionParams {
  resolve?: number;
  direction: TypeHierarchyDirection;
}

interface TypeHierarchyItem {
  name: string;
  detail?: string;
  kind: vscodelc.SymbolKind;
  deprecated?: boolean;
  uri: string;
  range: vscodelc.Range;
  selectionRange: vscodelc.Range;
  parents?: TypeHierarchyItem[];
  children?: TypeHierarchyItem[];
  data?: any;
}

export namespace TypeHierarchyRequest {
export const type =
    new vscodelc
        .RequestType<TypeHierarchyParams, TypeHierarchyItem|null, void, void>(
            'textDocument/typeHierarchy');
}

interface ResolveTypeHierarchyItemParams {
  item: TypeHierarchyItem;
  resolve: number;
  direction: TypeHierarchyDirection;
}

export namespace ResolveTypeHierarchyRequest {
export const type =
    new vscodelc.RequestType<ResolveTypeHierarchyItemParams,
                             TypeHierarchyItem|null, void, void>(
        'typeHierarchy/resolve');
}

class TypeHierarchyTreeItem extends vscode.TreeItem {
  constructor(item: TypeHierarchyItem) {
    // expand all for now
    super(item.name, vscode.TreeItemCollapsibleState.Expanded)
  }
}

export class TypeHierarchyProvider implements
    vscode.TreeDataProvider<TypeHierarchyItem> {

  private _onDidChangeTreeData: vscode.EventEmitter<TypeHierarchyItem> =
      new vscode.EventEmitter();
  readonly onDidChangeTreeData: vscode.Event<TypeHierarchyItem | undefined | null> =
      this._onDidChangeTreeData.event;

  private currentItem?: TypeHierarchyItem;

  constructor(context: vscode.ExtensionContext,
              private client: vscodelc.LanguageClient) {
    // Register the tree provider
    context.subscriptions.push(vscode.window.registerTreeDataProvider(
        'clangd-vscode.typeHierarchyView', this));

    // Add comand to refresh on new item
    context.subscriptions.push(vscode.commands.registerTextEditorCommand(
        'clangd-vscode.typeHierarchy', editor => this.openTypeHierarchy(editor)));
  }

  getTreeItem(element: TypeHierarchyItem): vscode.TreeItem
      |Thenable<vscode.TreeItem> {
    return new TypeHierarchyTreeItem(element);
  }

  getChildren(element?: TypeHierarchyItem):
      vscode.ProviderResult<TypeHierarchyItem[]> {
    if (element) {
      // TODO handle different directions
      if (element.parents) {
        return Promise.resolve(element.parents);
      } else {
        // TODO call to resolve item if we think there are more
        return Promise.resolve([]);
      }
    } else if (this.currentItem) {
      // Root item
      return Promise.resolve([ this.currentItem ]);
    } else {
      // No items yet
      return Promise.resolve([]);
    }
  }

  private async openTypeHierarchy(editor: vscode.TextEditor) {
    const item = await this.client.sendRequest(TypeHierarchyRequest.type, {
      ...this.client.code2ProtocolConverter.asTextDocumentPositionParams(
          editor.document, editor.selection.active),
      // TODO do something more proper
      resolve : 5,
      // TODO support different directions
      direction : TypeHierarchyDirection.Both
    });
    if (item) {
      this.currentItem = item;
      this._onDidChangeTreeData.fire(item);
    } else {
      vscode.window.showInformationMessage(
          'No type hierarchy available for selection');
    }
  }
}