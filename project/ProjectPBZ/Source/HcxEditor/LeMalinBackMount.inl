// Generate native blueprint calls only; the game needs no project runtime DLL.
static FString BuildLeMalinBackMount(UAnimBlueprint* BP)
{
 using namespace HcxPanel;
 FEdGraphPinType Object;Object.PinCategory=UEdGraphSchema_K2::PC_Object;Object.PinSubCategoryObject=USceneComponent::StaticClass();
 FEdGraphPinType Vector;Vector.PinCategory=UEdGraphSchema_K2::PC_Struct;Vector.PinSubCategoryObject=TBaseStructure<FVector>::Get();
 FEdGraphPinType Name;Name.PinCategory=UEdGraphSchema_K2::PC_Name;
 FEdGraphPinType Integer;Integer.PinCategory=UEdGraphSchema_K2::PC_Int;
 auto Add=[&](FName N,FEdGraphPinType T,bool Array=false){if(Array)T.ContainerType=EPinContainerType::Array;FBlueprintEditorUtils::AddMemberVariable(BP,N,T);};
 Add(TEXT("BackComponents"),Object,true);Add(TEXT("BackParents"),Object,true);Add(TEXT("BackSockets"),Name,true);Add(TEXT("BackBases"),Vector,true);Add(TEXT("BackApplied"),Vector,true);
 Add(TEXT("BackCandidate"),Object);Add(TEXT("BackCurrent"),Vector);Add(TEXT("BackChosenBase"),Vector);Add(TEXT("BackIndex"),Integer);Add(TEXT("BackFound"),Integer);
 Add(TEXT("BackHiddenComponents"),Object,true);Add(TEXT("BackHiddenRoots"),Object,true);Add(TEXT("BackHideIndex"),Integer);
 Add(TEXT("BackHideParts"),Object,true);
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
 UEdGraph* E=BP->UbergraphPages[0];Graph G{BP,E};UK2Node_Event* Update=nullptr;
 for(UEdGraphNode* N:E->Nodes)if(auto* Event=Cast<UK2Node_Event>(N))if(Event->EventReference.GetMemberName()==TEXT("BlueprintUpdateAnimation"))Update=Event;
 if(!Update)return TEXT("back mount update missing");
 auto Op=[&](FName F,UEdGraphPin* A,UEdGraphPin* B){auto* N=G.Call(UKismetMathLibrary::StaticClass(),F);G.Link(A,G.P(N,TEXT("A")));G.Link(B,G.P(N,TEXT("B")));return G.Ret(N);};
 auto Not=[&](UEdGraphPin* A){auto* N=G.Call(UKismetMathLibrary::StaticClass(),TEXT("Not_PreBool"));G.Link(A,G.P(N,TEXT("A")));return G.Ret(N);};
 auto And=[&](UEdGraphPin* A,UEdGraphPin* B){return Op(TEXT("BooleanAND"),A,B);};auto Or=[&](UEdGraphPin* A,UEdGraphPin* B){return Op(TEXT("BooleanOR"),A,B);};
 auto Method=[&](FName F,UEdGraphPin* Object){auto* N=G.Call(USceneComponent::StaticClass(),F);G.Link(Object,G.P(N,UEdGraphSchema_K2::PN_Self));return N;};
 auto Set=[&](UEdGraphPin* X,FName N,UEdGraphPin* Value){auto* S=G.Set(N);G.Link(Value,G.P(S,N));return G.Run(X,S);};
 auto Zero=[&](UEdGraphPin* X,FName N){auto* S=G.Set(N);G.Lit(S,N,TEXT("0"));return G.Run(X,S);};
 auto Join=[&](){FGraphNodeCreator<UK2Node_ExecutionSequence> C(*E);auto* N=C.CreateNode();C.Finalize();return TPair<UEdGraphPin*,UEdGraphPin*>(G.P(N,UEdGraphSchema_K2::PN_Execute),N->GetThenPinGivenIndex(0));};
 auto Array=[&](FName Function,FName Member){FGraphNodeCreator<UK2Node_CallArrayFunction> C(*E);auto* N=C.CreateNode();N->SetFromFunction(UKismetArrayLibrary::StaticClass()->FindFunctionByName(Function));C.Finalize();G.Link(G.Get(Member),G.P(N,TEXT("TargetArray")));return N;};
 auto Item=[&](FName Member,UEdGraphPin* Index){FGraphNodeCreator<UK2Node_GetArrayItem> C(*E);auto* N=C.CreateNode();C.Finalize();G.Link(G.Get(Member),N->GetTargetArrayPin());G.Link(Index,N->GetIndexPin());return N->GetResultPin();};
 auto Append=[&](UEdGraphPin* X,FName Member,UEdGraphPin* Value){auto* N=Array(TEXT("Array_Add"),Member);G.Link(Value,G.P(N,TEXT("NewItem")));return TPair<UEdGraphPin*,UEdGraphPin*>(G.Run(X,N),G.Ret(N));};
 auto Store=[&](UEdGraphPin* X,FName Member,UEdGraphPin* Index,UEdGraphPin* Value){auto* N=Array(TEXT("Array_Set"),Member);G.Link(Index,G.P(N,TEXT("Index")));G.Link(Value,G.P(N,TEXT("Item")));G.Lit(N,TEXT("bSizeToFit"),TEXT("false"));return G.Run(X,N);};
 auto Current=G.Get(TEXT("BackCandidate"));auto* Parent=Method(TEXT("GetAttachParent"),Current);auto* Socket=Method(TEXT("GetAttachSocketName"),Current);
 auto* String=G.Call(UKismetStringLibrary::StaticClass(),TEXT("Conv_NameToString"));G.Link(G.Ret(Socket),G.P(String,TEXT("InName")));
 auto Contains=[&](const TCHAR* S){auto* N=G.Call(UKismetStringLibrary::StaticClass(),TEXT("Contains"));G.Link(G.Ret(String),G.P(N,TEXT("SearchIn")));G.Lit(N,TEXT("Substring"),S);G.Lit(N,TEXT("bUseCase"),TEXT("false"));return G.Ret(N);};
 auto Back=And(And(Contains(TEXT("Pt_")),Contains(TEXT("_Back"))),Or(Or(Contains(TEXT("Blade")),Contains(TEXT("Knife"))),Or(Contains(TEXT("Chakram")),Contains(TEXT("SnakeDual")))));
 auto* Own=G.Call(UAnimInstance::StaticClass(),TEXT("GetOwningComponent"));auto* SourceParent=Method(TEXT("GetAttachParent"),G.Ret(Own));auto* Source=G.Cast(G.Ret(SourceParent),USkeletalMeshComponent::StaticClass());
 auto* Start=G.P(Update,UEdGraphSchema_K2::PN_Then);auto Old=Start->LinkedTo;Start->BreakAllPinLinks();FGraphNodeCreator<UK2Node_ExecutionSequence> SC(*E);auto* Seq=SC.CreateNode();SC.Finalize();G.Link(Start,G.P(Seq,UEdGraphSchema_K2::PN_Execute));for(auto* L:Old)G.Link(Seq->GetThenPinGivenIndex(0),L);
 // The panel is created for the local player's source mesh only. A valid panel
 // also lets the native headless fixture exercise this exact runtime graph.
 auto* Gate=G.Branch(Seq->GetThenPinGivenIndex(1),And(G.Valid(Source),G.Valid(G.Get(TEXT("LeMalinPanel")))));
 auto* ShowBack=Op(TEXT("GreaterEqual_DoubleDouble"),G.Get(TEXT("LeMalinBackVisible")),G.Number(.5));
 auto Snapshot=[&](UEdGraphPin* X,UEdGraphPin* Component){X=Set(X,TEXT("BackCandidate"),Component);auto* T=Method(TEXT("GetRelativeTransform"),Current);X=G.Run(X,T);auto* B=G.Call(UKismetMathLibrary::StaticClass(),TEXT("BreakTransform"));G.Link(G.Ret(T),G.P(B,TEXT("InTransform")));return Set(X,TEXT("BackCurrent"),G.P(B,TEXT("Location")));};
 auto Move=[&](UEdGraphPin* X,UEdGraphPin* Value){auto* N=Method(TEXT("K2_SetRelativeLocation"),Current);G.Link(Value,G.P(N,TEXT("NewLocation")));G.Lit(N,TEXT("bSweep"),TEXT("false"));G.Lit(N,TEXT("bTeleport"),TEXT("true"));return G.Run(X,N);};
 auto Near=[&](UEdGraphPin* A,UEdGraphPin* B){auto* D=G.Call(UKismetMathLibrary::StaticClass(),TEXT("Vector_Distance"));G.Link(A,G.P(D,TEXT("V1")));G.Link(B,G.P(D,TEXT("V2")));return Op(TEXT("Less_DoubleDouble"),G.Ret(D),G.Number(.025));};
 auto Index=G.Get(TEXT("BackIndex"));auto Found=G.Get(TEXT("BackFound"));
 auto Increment=[&](UEdGraphPin* X){auto* N=G.Call(UKismetMathLibrary::StaticClass(),TEXT("Add_IntInt"));G.Link(Index,G.P(N,TEXT("A")));G.Lit(N,TEXT("B"),TEXT("1"));return Set(X,TEXT("BackIndex"),G.Ret(N));};
 // First remove our previous offset on reattachment. If the game supplied a
 // new transform (e.g. SnapToTarget on draw), leave that transform untouched.
 auto* Count=Array(TEXT("Array_Length"),TEXT("BackComponents"));auto* Loop=G.Branch(Zero(G.P(Gate,UEdGraphSchema_K2::PN_Then),TEXT("BackIndex")),Op(TEXT("Less_IntInt"),Index,G.Ret(Count)));
 auto EndStep=Join();auto* Cached=Item(TEXT("BackComponents"),Index);auto* Valid=G.Branch(G.P(Loop,UEdGraphSchema_K2::PN_Then),G.Valid(Cached));G.Link(G.P(Valid,UEdGraphSchema_K2::PN_Else),EndStep.Key);
 auto* X=Snapshot(G.P(Valid,UEdGraphSchema_K2::PN_Then),Cached);auto* Base=Item(TEXT("BackBases"),Index);auto* Last=Item(TEXT("BackApplied"),Index);
 auto* SameParent=Op(TEXT("EqualEqual_ObjectObject"),G.Ret(Parent),Item(TEXT("BackParents"),Index));auto* SameSocket=Op(TEXT("EqualEqual_NameName"),G.Ret(Socket),Item(TEXT("BackSockets"),Index));
 auto* NeedsRestore=And(Not(Near(Base,Last)),Not(And(Back,And(SameParent,SameSocket))));auto* Restore=G.Branch(X,NeedsRestore);G.Link(G.P(Restore,UEdGraphSchema_K2::PN_Else),EndStep.Key);
 auto* Unchanged=G.Branch(G.P(Restore,UEdGraphSchema_K2::PN_Then),Near(G.Get(TEXT("BackCurrent")),Last));auto Restored=Join();
 X=Move(G.P(Unchanged,UEdGraphSchema_K2::PN_Then),Base);X=Set(X,TEXT("BackCurrent"),Base);G.Link(X,Restored.Key);G.Link(G.P(Unchanged,UEdGraphSchema_K2::PN_Else),Restored.Key);
 X=Store(Restored.Value,TEXT("BackBases"),Index,G.Get(TEXT("BackCurrent")));X=Store(X,TEXT("BackApplied"),Index,G.Get(TEXT("BackCurrent")));X=Store(X,TEXT("BackParents"),Index,G.Ret(Parent));X=Store(X,TEXT("BackSockets"),Index,G.Ret(Socket));G.Link(X,EndStep.Key);G.Link(Increment(EndStep.Value),G.P(Loop,UEdGraphSchema_K2::PN_Execute));
 // Find all actual back-mounted roots. Children without a back socket inherit
 // their parent's shift, so a blade/scabbard subtree is not shifted twice.
 auto* Children=Method(TEXT("GetChildrenComponents"),Source);G.Lit(Children,TEXT("bIncludeAllDescendants"),TEXT("true"));X=G.Run(G.P(Loop,UEdGraphSchema_K2::PN_Else),Children);
 FGraphNodeCreator<UK2Node_CallArrayFunction> NC(*E);auto* Number=NC.CreateNode();Number->SetFromFunction(UKismetArrayLibrary::StaticClass()->FindFunctionByName(TEXT("Array_Length")));NC.Finalize();G.Link(G.P(Children,TEXT("Children")),G.P(Number,TEXT("TargetArray")));
 auto* Scan=G.Branch(Zero(X,TEXT("BackIndex")),Op(TEXT("Less_IntInt"),Index,G.Ret(Number)));auto ScanStep=Join();
 FGraphNodeCreator<UK2Node_GetArrayItem> C(*E);auto* Child=C.CreateNode();C.Finalize();G.Link(G.P(Children,TEXT("Children")),Child->GetTargetArrayPin());G.Link(Index,Child->GetIndexPin());auto* ChildValid=G.Branch(G.P(Scan,UEdGraphSchema_K2::PN_Then),G.Valid(Child->GetResultPin()));G.Link(G.P(ChildValid,UEdGraphSchema_K2::PN_Else),ScanStep.Key);
 X=Snapshot(G.P(ChildValid,UEdGraphSchema_K2::PN_Then),Child->GetResultPin());auto* IsBack=G.Branch(X,And(Back,G.Valid(G.Ret(Parent))));G.Link(G.P(IsBack,UEdGraphSchema_K2::PN_Else),ScanStep.Key);
 auto* Find=Array(TEXT("Array_Find"),TEXT("BackComponents"));G.Link(Current,G.P(Find,TEXT("ItemToFind")));X=Set(G.P(IsBack,UEdGraphSchema_K2::PN_Then),TEXT("BackFound"),G.Ret(Find));
 auto* HasCache=G.Branch(X,Op(TEXT("GreaterEqual_IntInt"),Found,G.Number(0)));auto CacheReady=Join();G.Link(G.P(HasCache,UEdGraphSchema_K2::PN_Then),CacheReady.Key);
 auto New=Append(G.P(HasCache,UEdGraphSchema_K2::PN_Else),TEXT("BackComponents"),Current);X=Set(New.Key,TEXT("BackFound"),New.Value);
 for(FName N:{TEXT("BackBases"),TEXT("BackApplied")})X=Append(X,N,G.Get(TEXT("BackCurrent"))).Key;
 X=Append(X,TEXT("BackParents"),G.Ret(Parent)).Key;X=Append(X,TEXT("BackSockets"),G.Ret(Socket)).Key;G.Link(X,CacheReady.Key);
 auto* Same=G.Branch(CacheReady.Value,Near(G.Get(TEXT("BackCurrent")),Item(TEXT("BackApplied"),Found)));auto BaseReady=Join();
 X=Set(G.P(Same,UEdGraphSchema_K2::PN_Then),TEXT("BackChosenBase"),Item(TEXT("BackBases"),Found));G.Link(X,BaseReady.Key);
 X=Set(G.P(Same,UEdGraphSchema_K2::PN_Else),TEXT("BackChosenBase"),G.Get(TEXT("BackCurrent")));X=Store(X,TEXT("BackBases"),Found,G.Get(TEXT("BackChosenBase")));G.Link(X,BaseReady.Key);
 auto* Owner=G.Call(UActorComponent::StaticClass(),TEXT("GetOwner"));G.Link(Source,G.P(Owner,UEdGraphSchema_K2::PN_Self));auto* Forward=G.Call(AActor::StaticClass(),TEXT("GetActorForwardVector"));G.Link(G.Ret(Owner),G.P(Forward,UEdGraphSchema_K2::PN_Self));
 auto* Distance=Op(TEXT("Multiply_DoubleDouble"),G.Get(TEXT("LeMalinBackDistance")),G.Number(-1));auto* WorldDelta=Op(TEXT("Multiply_VectorFloat"),G.Ret(Forward),Distance);
 auto* SocketTM=Method(TEXT("GetSocketTransform"),G.Ret(Parent));G.Link(G.Ret(Socket),G.P(SocketTM,TEXT("InSocketName")));G.Lit(SocketTM,TEXT("TransformSpace"),TEXT("RTS_World"));
 X=G.Run(BaseReady.Value,Owner);X=G.Run(X,Forward);X=G.Run(X,SocketTM);auto* Split=G.Call(UKismetMathLibrary::StaticClass(),TEXT("BreakTransform"));G.Link(G.Ret(SocketTM),G.P(Split,TEXT("InTransform")));
 auto* Inverse=G.Call(UKismetMathLibrary::StaticClass(),TEXT("InverseTransformLocation"));G.Link(G.Ret(SocketTM),G.P(Inverse,TEXT("T")));G.Link(Op(TEXT("Add_VectorVector"),G.P(Split,TEXT("Location")),WorldDelta),G.P(Inverse,TEXT("Location")));
 auto* Desired=Op(TEXT("Add_VectorVector"),G.Get(TEXT("BackChosenBase")),G.Ret(Inverse));X=Move(X,Desired);X=Store(X,TEXT("BackApplied"),Found,Desired);X=Store(X,TEXT("BackParents"),Found,G.Ret(Parent));X=Store(X,TEXT("BackSockets"),Found,G.Ret(Socket));G.Link(X,ScanStep.Key);G.Link(Increment(ScanStep.Value),G.P(Scan,UEdGraphSchema_K2::PN_Execute));
 // Remember only components we actually hid. Restore them when shown, drawn,
 // or reattached; components already hidden by the game remain untouched.
 auto Visibility=[&](UEdGraphPin* In,UEdGraphPin* Component,bool Show){
  // Captured components were visible, so their original HiddenInGame was false.
  auto* Hidden=Method(TEXT("SetHiddenInGame"),Component);
  G.Lit(Hidden,TEXT("NewHidden"),Show?TEXT("false"):TEXT("true"));
  G.Lit(Hidden,TEXT("bPropagateToChildren"),TEXT("false"));
  auto* Visible=Method(TEXT("SetVisibility"),Component);
  G.Lit(Visible,TEXT("bNewVisibility"),Show?TEXT("true"):TEXT("false"));
  G.Lit(Visible,TEXT("bPropagateToChildren"),TEXT("false"));
  return G.Run(G.Run(In,Hidden),Visible);
 };
 auto* HiddenCount=Array(TEXT("Array_Length"),TEXT("BackHiddenComponents"));auto* Clean=G.Branch(Zero(G.P(Scan,UEdGraphSchema_K2::PN_Else),TEXT("BackIndex")),Op(TEXT("Less_IntInt"),Index,G.Ret(HiddenCount)));
 auto* Hidden=Item(TEXT("BackHiddenComponents"),Index);auto* HiddenRoot=Item(TEXT("BackHiddenRoots"),Index);auto Remove=Join();auto Keep=Join();
 auto* HiddenValid=G.Branch(G.P(Clean,UEdGraphSchema_K2::PN_Then),G.Valid(Hidden));G.Link(G.P(HiddenValid,UEdGraphSchema_K2::PN_Else),Remove.Key);
 auto* RootValid=G.Branch(G.P(HiddenValid,UEdGraphSchema_K2::PN_Then),G.Valid(HiddenRoot));auto RestoreVisible=Join();G.Link(G.P(RootValid,UEdGraphSchema_K2::PN_Else),RestoreVisible.Key);
 X=Set(G.P(RootValid,UEdGraphSchema_K2::PN_Then),TEXT("BackCandidate"),HiddenRoot);auto* StayHidden=G.Branch(X,And(Not(ShowBack),Back));G.Link(G.P(StayHidden,UEdGraphSchema_K2::PN_Else),RestoreVisible.Key);
 auto* Descendants=Method(TEXT("GetChildrenComponents"),HiddenRoot);G.Lit(Descendants,TEXT("bIncludeAllDescendants"),TEXT("true"));X=G.Run(G.P(StayHidden,UEdGraphSchema_K2::PN_Then),Descendants);
 FGraphNodeCreator<UK2Node_CallArrayFunction> CC(*E);auto* ContainsChild=CC.CreateNode();ContainsChild->SetFromFunction(UKismetArrayLibrary::StaticClass()->FindFunctionByName(TEXT("Array_Contains")));CC.Finalize();G.Link(G.P(Descendants,TEXT("Children")),G.P(ContainsChild,TEXT("TargetArray")));G.Link(Hidden,G.P(ContainsChild,TEXT("ItemToFind")));
 auto* StillBack=G.Branch(X,Or(Op(TEXT("EqualEqual_ObjectObject"),Hidden,HiddenRoot),G.Ret(ContainsChild)));G.Link(G.P(StillBack,UEdGraphSchema_K2::PN_Then),Keep.Key);G.Link(G.P(StillBack,UEdGraphSchema_K2::PN_Else),RestoreVisible.Key);
 G.Link(Visibility(RestoreVisible.Value,Hidden,true),Remove.Key);
 X=Remove.Value;for(FName N:{TEXT("BackHiddenComponents"),TEXT("BackHiddenRoots")}){auto* R=Array(TEXT("Array_Remove"),N);G.Link(Index,G.P(R,TEXT("IndexToRemove")));X=G.Run(X,R);}G.Link(X,G.P(Clean,UEdGraphSchema_K2::PN_Execute));G.Link(Increment(Visibility(Keep.Value,Hidden,false)),G.P(Clean,UEdGraphSchema_K2::PN_Execute));
 auto* HideGate=G.Branch(G.P(Clean,UEdGraphSchema_K2::PN_Else),Not(ShowBack));auto* Roots=G.Branch(Zero(G.P(HideGate,UEdGraphSchema_K2::PN_Then),TEXT("BackIndex")),Op(TEXT("Less_IntInt"),Index,G.Ret(Count)));auto RootStep=Join();
 auto* Root=Item(TEXT("BackComponents"),Index);auto* GoodRoot=G.Branch(G.P(Roots,UEdGraphSchema_K2::PN_Then),G.Valid(Root));G.Link(G.P(GoodRoot,UEdGraphSchema_K2::PN_Else),RootStep.Key);X=Set(G.P(GoodRoot,UEdGraphSchema_K2::PN_Then),TEXT("BackCandidate"),Root);auto* BackRoot=G.Branch(X,Back);G.Link(G.P(BackRoot,UEdGraphSchema_K2::PN_Else),RootStep.Key);
 auto* Parts=Method(TEXT("GetChildrenComponents"),Root);G.Lit(Parts,TEXT("bIncludeAllDescendants"),TEXT("true"));X=G.Run(G.P(BackRoot,UEdGraphSchema_K2::PN_Then),Parts);
 // Materialize the pure getter's result before modifying it. Each reevaluation
 // otherwise creates a fresh array and drops the root appended below.
 X=Set(X,TEXT("BackHideParts"),G.P(Parts,TEXT("Children")));
 auto PartArray=[&](FName Fn){return Array(Fn,TEXT("BackHideParts"));};
 auto* IncludeRoot=PartArray(TEXT("Array_Add"));G.Link(Root,G.P(IncludeRoot,TEXT("NewItem")));X=G.Run(X,IncludeRoot);auto* PartCount=PartArray(TEXT("Array_Length"));auto* PartIndex=G.Get(TEXT("BackHideIndex"));auto* PartLoop=G.Branch(Zero(X,TEXT("BackHideIndex")),Op(TEXT("Less_IntInt"),PartIndex,G.Ret(PartCount)));auto PartStep=Join();G.Link(G.P(PartLoop,UEdGraphSchema_K2::PN_Else),RootStep.Key);
 FGraphNodeCreator<UK2Node_GetArrayItem> PC(*E);auto* Part=PC.CreateNode();PC.Finalize();G.Link(G.Get(TEXT("BackHideParts")),Part->GetTargetArrayPin());G.Link(PartIndex,Part->GetIndexPin());auto* PartValue=Part->GetResultPin();auto* GoodPart=G.Branch(G.P(PartLoop,UEdGraphSchema_K2::PN_Then),G.Valid(PartValue));G.Link(G.P(GoodPart,UEdGraphSchema_K2::PN_Else),PartStep.Key);
 auto* IsVisible=Method(TEXT("IsVisible"),PartValue);auto* WasVisible=G.Branch(G.Run(G.P(GoodPart,UEdGraphSchema_K2::PN_Then),IsVisible),G.Ret(IsVisible));G.Link(G.P(WasVisible,UEdGraphSchema_K2::PN_Else),PartStep.Key);
 auto* Remember=Array(TEXT("Array_AddUnique"),TEXT("BackHiddenComponents"));G.Link(PartValue,G.P(Remember,TEXT("NewItem")));X=G.Run(G.P(WasVisible,UEdGraphSchema_K2::PN_Then),Remember);auto* RememberRoot=Array(TEXT("Array_Set"),TEXT("BackHiddenRoots"));G.Link(G.Ret(Remember),G.P(RememberRoot,TEXT("Index")));G.Link(Root,G.P(RememberRoot,TEXT("Item")));G.Lit(RememberRoot,TEXT("bSizeToFit"),TEXT("true"));X=G.Run(X,RememberRoot);G.Link(Visibility(X,PartValue,false),PartStep.Key);
 auto* NextPart=G.Call(UKismetMathLibrary::StaticClass(),TEXT("Add_IntInt"));G.Link(PartIndex,G.P(NextPart,TEXT("A")));G.Lit(NextPart,TEXT("B"),TEXT("1"));G.Link(Set(PartStep.Value,TEXT("BackHideIndex"),G.Ret(NextPart)),G.P(PartLoop,UEdGraphSchema_K2::PN_Execute));G.Link(Increment(RootStep.Value),G.P(Roots,UEdGraphSchema_K2::PN_Execute));
 if(!G.OK)return G.Error;FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);BP->MarkPackageDirty();return BP->Status==BS_Error?TEXT("back mount graph compile failed"):TEXT("OK: back offset and visibility, draw/sheath cleanup");
}
