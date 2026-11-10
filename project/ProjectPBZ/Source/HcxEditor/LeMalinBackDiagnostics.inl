// Capture the game's actual attachment tree into the existing settings save.
// Collection runs while the tuning panel is open.
static FString BuildLeMalinBackDiagnostics(UAnimBlueprint* BP)
{
 using namespace HcxPanel;
 FEdGraphPinType String;String.PinCategory=UEdGraphSchema_K2::PC_String;
 FEdGraphPinType Int;Int.PinCategory=UEdGraphSchema_K2::PC_Int;
 FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("BackDiagnosticText"),String);
 FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("BackDiagnosticIndex"),Int);
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
 auto* WidgetClass=LoadClass<UUserWidget>(nullptr,TEXT("/Game/LeMalin/WBP_LeMalinTuning.WBP_LeMalinTuning_C"));
 if(!WidgetClass)return TEXT("back diagnostic widget missing");
 UEdGraph* E=BP->UbergraphPages[0];Graph G{BP,E};UK2Node_Event* Update=nullptr;
 for(UEdGraphNode* N:E->Nodes)if(auto* Event=Cast<UK2Node_Event>(N))if(Event->EventReference.GetMemberName()==TEXT("BlueprintUpdateAnimation"))Update=Event;
 if(!Update)return TEXT("back diagnostic update missing");
 auto Join=[&](){FGraphNodeCreator<UK2Node_ExecutionSequence> C(*E);auto* N=C.CreateNode();C.Finalize();return TPair<UEdGraphPin*,UEdGraphPin*>(G.P(N,UEdGraphSchema_K2::PN_Execute),N->GetThenPinGivenIndex(0));};
 auto Set=[&](UEdGraphPin* Exec,FName Name,UEdGraphPin* Value){auto* N=G.Set(Name);G.Link(Value,G.P(N,Name));return G.Run(Exec,N);};
 auto Literal=[&](const TCHAR* Value){auto* N=G.Call(UKismetSystemLibrary::StaticClass(),TEXT("MakeLiteralString"));G.Lit(N,TEXT("Value"),Value);return G.Ret(N);};
 auto Path=[&](UEdGraphPin* Object){auto* N=G.Call(UKismetSystemLibrary::StaticClass(),TEXT("GetPathName"));G.Link(Object,G.P(N,TEXT("Object")));return G.Ret(N);};
 auto BoolText=[&](UEdGraphPin* Value){auto* N=G.Call(UKismetStringLibrary::StaticClass(),TEXT("Conv_BoolToString"));G.Link(Value,G.P(N,TEXT("InBool")));return G.Ret(N);};
 auto Concat=[&](UEdGraphPin* A,UEdGraphPin* B){auto* N=G.Call(UKismetStringLibrary::StaticClass(),TEXT("Concat_StrStr"));G.Link(A,G.P(N,TEXT("A")));G.Link(B,G.P(N,TEXT("B")));return G.Ret(N);};
 auto Field=[&](UEdGraphPin* Text,const TCHAR* Label,UEdGraphPin* Value){return Concat(Concat(Text,Literal(Label)),Value);};
 auto Method=[&](FName Name,UEdGraphPin* Object){auto* N=G.Call(USceneComponent::StaticClass(),Name);G.Link(Object,G.P(N,UEdGraphSchema_K2::PN_Self));return N;};
 auto* Start=G.P(Update,UEdGraphSchema_K2::PN_Then);auto Old=Start->LinkedTo;Start->BreakAllPinLinks();
 FGraphNodeCreator<UK2Node_ExecutionSequence> SC(*E);auto* Seq=SC.CreateNode();SC.Finalize();G.Link(Start,G.P(Seq,UEdGraphSchema_K2::PN_Execute));for(auto* Pin:Old)G.Link(Seq->GetThenPinGivenIndex(0),Pin);
 auto* Widget=G.Cast(G.Get(TEXT("LeMalinPanel")),WidgetClass);
 auto* HasWidget=G.Branch(Seq->GetThenPinGivenIndex(1),G.Valid(Widget));
 auto* PanelVisible=G.Call(UWidget::StaticClass(),TEXT("IsVisible"));G.Link(G.Get(TEXT("ControlsPanel"),WidgetClass,Widget),G.P(PanelVisible,UEdGraphSchema_K2::PN_Self));
 auto* Open=G.Branch(G.P(HasWidget,UEdGraphSchema_K2::PN_Then),G.Ret(PanelVisible));
 auto* Own=G.Call(UAnimInstance::StaticClass(),TEXT("GetOwningComponent"));auto* Parent=Method(TEXT("GetAttachParent"),G.Ret(Own));
 auto* Source=G.Cast(G.Ret(Parent),USkeletalMeshComponent::StaticClass());
 auto* ValidSource=G.Branch(G.P(Open,UEdGraphSchema_K2::PN_Then),G.Valid(Source));
 auto* Children=Method(TEXT("GetChildrenComponents"),Source);G.Lit(Children,TEXT("bIncludeAllDescendants"),TEXT("true"));
 auto* Exec=G.Run(G.P(ValidSource,UEdGraphSchema_K2::PN_Then),Children);
 Exec=Set(Exec,TEXT("BackDiagnosticText"),Field(Literal(TEXT("SOURCE")),TEXT(" "),Path(Source)));
 // Both descendants and the player's own components are useful for native tools.
 auto Collect=[&](UEdGraphPin* In,UEdGraphPin* Array,const TCHAR* Heading){
  In=Set(In,TEXT("BackDiagnosticText"),Concat(G.Get(TEXT("BackDiagnosticText")),Literal(Heading)));
  In=Set(In,TEXT("BackDiagnosticIndex"),G.Number(0));
  FGraphNodeCreator<UK2Node_CallArrayFunction> LC(*E);auto* Length=LC.CreateNode();Length->SetFromFunction(UKismetArrayLibrary::StaticClass()->FindFunctionByName(TEXT("Array_Length")));LC.Finalize();G.Link(Array,G.P(Length,TEXT("TargetArray")));
  auto* Less=G.Call(UKismetMathLibrary::StaticClass(),TEXT("Less_IntInt"));G.Link(G.Get(TEXT("BackDiagnosticIndex")),G.P(Less,TEXT("A")));G.Link(G.Ret(Length),G.P(Less,TEXT("B")));
  auto* Loop=G.Branch(In,G.Ret(Less));auto Next=Join();
  FGraphNodeCreator<UK2Node_GetArrayItem> IC(*E);auto* Item=IC.CreateNode();IC.Finalize();G.Link(Array,Item->GetTargetArrayPin());G.Link(G.Get(TEXT("BackDiagnosticIndex")),Item->GetIndexPin());
  auto* Component=G.Cast(Item->GetResultPin(),USceneComponent::StaticClass());auto* Valid=G.Branch(G.P(Loop,UEdGraphSchema_K2::PN_Then),G.Valid(Component));G.Link(G.P(Valid,UEdGraphSchema_K2::PN_Else),Next.Key);
  auto* Attach=Method(TEXT("GetAttachParent"),Component);auto* Socket=Method(TEXT("GetAttachSocketName"),Component);
  auto* SocketString=G.Call(UKismetStringLibrary::StaticClass(),TEXT("Conv_NameToString"));G.Link(G.Ret(Socket),G.P(SocketString,TEXT("InName")));
  auto* Visible=Method(TEXT("IsVisible"),Component);
  auto* Text=Field(G.Get(TEXT("BackDiagnosticText")),TEXT("\nOBJECT "),Path(Component));
  Text=Field(Text,TEXT(" | parent="),Path(G.Ret(Attach)));Text=Field(Text,TEXT(" | socket="),G.Ret(SocketString));
  Text=Field(Text,TEXT(" | visible="),BoolText(G.Ret(Visible)));Text=Field(Text,TEXT(" | hidden="),BoolText(G.Get(TEXT("bHiddenInGame"),USceneComponent::StaticClass(),Component)));
  auto* X=Set(G.P(Valid,UEdGraphSchema_K2::PN_Then),TEXT("BackDiagnosticText"),Text);
  auto* Mesh=G.Cast(Component,UMeshComponent::StaticClass());auto* HasMesh=G.Branch(X,G.Valid(Mesh));G.Link(G.P(HasMesh,UEdGraphSchema_K2::PN_Else),Next.Key);
  auto* Material=G.Call(UPrimitiveComponent::StaticClass(),TEXT("GetMaterial"));G.Link(Mesh,G.P(Material,UEdGraphSchema_K2::PN_Self));
  X=Set(G.P(HasMesh,UEdGraphSchema_K2::PN_Then),TEXT("BackDiagnosticText"),Field(G.Get(TEXT("BackDiagnosticText")),TEXT(" | material0="),Path(G.Ret(Material))));
  auto* Skinned=G.Cast(Component,USkinnedMeshComponent::StaticClass());auto* HasSkinned=G.Branch(X,G.Valid(Skinned));G.Link(G.P(HasSkinned,UEdGraphSchema_K2::PN_Else),Next.Key);
  auto* Asset=G.Call(USkinnedMeshComponent::StaticClass(),TEXT("GetSkinnedAsset"));G.Link(Skinned,G.P(Asset,UEdGraphSchema_K2::PN_Self));
  X=Set(G.P(HasSkinned,UEdGraphSchema_K2::PN_Then),TEXT("BackDiagnosticText"),Field(G.Get(TEXT("BackDiagnosticText")),TEXT(" | skinned_asset="),Path(G.Ret(Asset))));G.Link(X,Next.Key);
  auto* Add=G.Call(UKismetMathLibrary::StaticClass(),TEXT("Add_IntInt"));G.Link(G.Get(TEXT("BackDiagnosticIndex")),G.P(Add,TEXT("A")));G.Lit(Add,TEXT("B"),TEXT("1"));
  G.Link(Set(Next.Value,TEXT("BackDiagnosticIndex"),G.Ret(Add)),G.P(Loop,UEdGraphSchema_K2::PN_Execute));
  return G.P(Loop,UEdGraphSchema_K2::PN_Else);
 };
 Exec=Collect(Exec,G.P(Children,TEXT("Children")),TEXT("\nDESCENDANTS"));
 auto* Owner=G.Call(UActorComponent::StaticClass(),TEXT("GetOwner"));G.Link(Source,G.P(Owner,UEdGraphSchema_K2::PN_Self));
 auto* Components=G.Call(AActor::StaticClass(),TEXT("K2_GetComponentsByClass"));G.Link(G.Ret(Owner),G.P(Components,UEdGraphSchema_K2::PN_Self));G.Obj(Components,TEXT("ComponentClass"),USceneComponent::StaticClass());
 Exec=G.Run(Exec,Owner);Exec=G.Run(Exec,Components);Exec=Collect(Exec,G.Ret(Components),TEXT("\nPLAYER_COMPONENTS"));
 auto* Store=G.Set(TEXT("BackDiagnostics"),WidgetClass,Widget);G.Link(G.Get(TEXT("BackDiagnosticText")),G.P(Store,TEXT("BackDiagnostics")));G.Run(Exec,Store);
 if(!G.OK)return G.Error;FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);BP->MarkPackageDirty();
 return BP->Status==BS_Error?TEXT("back diagnostic graph compile failed"):TEXT("OK: back attachment capture");
}
