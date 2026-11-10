// Direct material swaps make the sunlight comparison independent of MID filters.
static FString BuildLeMalinMaterialTuning(UAnimBlueprint* BP)
{
 using namespace HcxPanel;
 const bool PlayerRelease=FPlatformMisc::GetEnvironmentVariable(TEXT("LEMALIN_PLAYER_RELEASE"))==TEXT("1");
 auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/LeMalin/SK_LeMalin_Lively"));if(!Mesh)return TEXT("material mesh missing");
 TArray<int32> Slots;const TSet<FString> Wanted={TEXT("Face"),TEXT("Body"),TEXT("Hair"),TEXT("HairFront"),TEXT("Cloth"),TEXT("Obj"),TEXT("Shoes"),TEXT("RabbitEar"),TEXT("Belt"),TEXT("Eye"),TEXT("EyeLight")};
 for(int32 I=0;I<Mesh->GetMaterials().Num();++I){FString N=Mesh->GetMaterials()[I].MaterialSlotName.ToString();N.RemoveFromStart(TEXT("Preview_"));if(Wanted.Contains(N))Slots.Add(I);}
 if(!Slots.Num())return TEXT("material slots missing");
 auto IsEyeSlot=[&](int32 Slot){
  FString Name=Mesh->GetMaterials()[Slot].MaterialSlotName.ToString();
  Name.RemoveFromStart(TEXT("Preview_"));
  return Name==TEXT("Eye")||Name==TEXT("EyeLight");
 };
 TArray<TArray<UMaterialInterface*>> Assets;
 TArray<TArray<UMaterialInterface*>> BlackAssets;
 for(int32 Slot:Slots){
  auto* Base=Mesh->GetMaterials()[Slot].MaterialInterface.Get();if(!Base)return TEXT("material missing");
  TArray<UMaterialInterface*> Row={Base};
  if(PlayerRelease){
   Row.Reset();
   if(IsEyeSlot(Slot)){
    for(const TCHAR* Name:{TEXT("MI_Eye_Gloss"),TEXT("MI_Eye_Gem")}){
     auto* Mat=LoadObject<UMaterialInterface>(nullptr,*(FString(TEXT("/Game/LeMalin/Materials/"))+Name));
     if(!Mat)return TEXT("player eye material missing");Row.Add(Mat);
    }
   }else{
    FString Part=Base->GetName();Part.RemoveFromStart(TEXT("MI_"));
    auto* Mat=LoadObject<UMaterialInterface>(nullptr,*(TEXT("/Game/LeMalin/Materials/MI_SunQuarter_")+Part));
    if(!Mat)return TEXT("player sunlight material missing");Row.Add(Mat);
   }
  }else if(IsEyeSlot(Slot)){
   for(const TCHAR* Name:{TEXT("MI_Eye_Gloss"),TEXT("MI_Eye_Crystal"),TEXT("MI_Eye_Gem")}){
    auto* Mat=LoadObject<UMaterialInterface>(nullptr,*(FString(TEXT("/Game/LeMalin/Materials/"))+Name));
    if(!Mat)return TEXT("eye finish material missing: ")+FString(Name);
    Row.Add(Mat);
   }
  }else{
   FString Part=Base->GetName();Part.RemoveFromStart(TEXT("MI_"));
   for(const TCHAR* Mode:{TEXT("Half"),TEXT("Quarter"),TEXT("Eighth"),TEXT("Sixteenth"),TEXT("Black")}){
    auto* Mat=LoadObject<UMaterialInterface>(nullptr,*(TEXT("/Game/LeMalin/Materials/MI_Sun")+FString(Mode)+TEXT("_")+Part));
    if(!Mat)return TEXT("sun comparison material missing: ")+Part;
    Row.Add(Mat);
   }
  }
  Assets.Add(Row);
  TArray<UMaterialInterface*> BlackRow;
  FString SlotName=Mesh->GetMaterials()[Slot].MaterialSlotName.ToString();SlotName.RemoveFromStart(TEXT("Preview_"));
  if(SlotName==TEXT("Cloth")||SlotName==TEXT("Obj")||SlotName==TEXT("RabbitEar")||SlotName==TEXT("Shoes")||SlotName==TEXT("Belt")){
   FString Part=SlotName==TEXT("Cloth")?TEXT("Cloth"):TEXT("Obj");
   if(PlayerRelease){
    FString Name=SlotName==TEXT("Cloth")?TEXT("MI_StyleSheer_Quarter_Cloth"):TEXT("MI_StyleBlack_Quarter_Obj");
    auto* Mat=LoadObject<UMaterialInterface>(nullptr,*(TEXT("/Game/LeMalin/Materials/")+Name));
    if(!Mat)return TEXT("player black material missing");BlackRow.Add(Mat);
   }else for(const TCHAR* Level:{TEXT("Original"),TEXT("Half"),TEXT("Quarter"),TEXT("Eighth"),TEXT("Sixteenth")}){
    auto* Mat=LoadObject<UMaterialInterface>(nullptr,*(TEXT("/Game/LeMalin/Materials/MI_StyleBlack_")+FString(Level)+TEXT("_")+Part));
    if(!Mat)return TEXT("black outfit material missing: ")+Part;
    BlackRow.Add(Mat);
   }
   if(!PlayerRelease)BlackRow.Add(Row.Last());
  }
  BlackAssets.Add(BlackRow);
 }
 TArray<UMaterialInterface*> SheerClothAssets;
 if(!PlayerRelease)for(const TCHAR* Level:{TEXT("Original"),TEXT("Half"),TEXT("Quarter"),TEXT("Eighth"),TEXT("Sixteenth")}){
  auto* Mat=LoadObject<UMaterialInterface>(nullptr,*(FString(TEXT("/Game/LeMalin/Materials/MI_StyleSheer_"))+Level+TEXT("_Cloth")));
  if(!Mat)return TEXT("thin stocking material missing");SheerClothAssets.Add(Mat);
 }
 if(!PlayerRelease)SheerClothAssets.Add(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/LeMalin/Materials/MI_SunBlack_Cloth")));
 TArray<TArray<UMaterialInterface*>> StockingProbes;
 if(!PlayerRelease)for(const TCHAR* Mode:{TEXT("Smooth"),TEXT("Matte"),TEXT("Flat"),TEXT("Ridges")}){
  TArray<UMaterialInterface*> Row;
  for(const TCHAR* Level:{TEXT("Original"),TEXT("Half"),TEXT("Quarter"),TEXT("Eighth"),TEXT("Sixteenth")}){
   auto* Mat=LoadObject<UMaterialInterface>(nullptr,*(FString(TEXT("/Game/LeMalin/Materials/MI_StockingProbe_"))+Mode+TEXT("_")+Level+TEXT("_Cloth")));
   if(!Mat)return TEXT("stocking probe material missing");Row.Add(Mat);
  }
  Row.Add(SheerClothAssets.Last());StockingProbes.Add(Row);
 }
 FEdGraphPinType Int;Int.PinCategory=UEdGraphSchema_K2::PC_Int;FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("SunTargetIndex"),Int);
 FEdGraphPinType Object;Object.PinCategory=UEdGraphSchema_K2::PC_Object;Object.PinSubCategoryObject=USkeletalMeshComponent::StaticClass();FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("SunTarget"),Object);
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
 UEdGraph* E=BP->UbergraphPages[0];Graph G{BP,E};UK2Node_Event* Update=nullptr;for(UEdGraphNode* N:E->Nodes)if(auto* Event=Cast<UK2Node_Event>(N))if(Event->EventReference.GetMemberName()==TEXT("BlueprintUpdateAnimation"))Update=Event;if(!Update)return TEXT("material update missing");
 auto Op=[&](FName F,UEdGraphPin* A,UEdGraphPin* B){auto* N=G.Call(UKismetMathLibrary::StaticClass(),F);G.Link(A,G.P(N,TEXT("A")));G.Link(B,G.P(N,TEXT("B")));return G.Ret(N);};
 auto Set=[&](UEdGraphPin* X,FName Name,UEdGraphPin* Value){auto* N=G.Set(Name);G.Link(Value,G.P(N,Name));return G.Run(X,N);};
 auto Join=[&](){FGraphNodeCreator<UK2Node_ExecutionSequence> C(*E);auto* N=C.CreateNode();C.Finalize();return TPair<UEdGraphPin*,UEdGraphPin*>(G.P(N,UEdGraphSchema_K2::PN_Execute),N->GetThenPinGivenIndex(0));};
 auto* Own=G.Call(UAnimInstance::StaticClass(),TEXT("GetOwningComponent"));auto* Parent=G.Call(USceneComponent::StaticClass(),TEXT("GetAttachParent"));G.Link(G.Ret(Own),G.P(Parent,UEdGraphSchema_K2::PN_Self));auto* Pick=G.Call(UKismetMathLibrary::StaticClass(),TEXT("SelectObject"));G.Link(G.Ret(Parent),G.P(Pick,TEXT("A")));G.Link(G.Ret(Own),G.P(Pick,TEXT("B")));G.Link(G.Valid(G.Ret(Parent)),G.P(Pick,TEXT("bSelectA")));
 auto* Start=G.P(Update,UEdGraphSchema_K2::PN_Then);auto Old=Start->LinkedTo;Start->BreakAllPinLinks();FGraphNodeCreator<UK2Node_ExecutionSequence> SC(*E);auto* Seq=SC.CreateNode();SC.Finalize();G.Link(Start,G.P(Seq,UEdGraphSchema_K2::PN_Execute));for(auto* L:Old)G.Link(Seq->GetThenPinGivenIndex(0),L);
 auto* Gate=G.Branch(Seq->GetThenPinGivenIndex(1),Op(TEXT("BooleanAND"),G.Valid(G.Ret(Own)),G.Valid(G.Get(TEXT("LeMalinPanel")))));
 auto* X=G.P(Gate,UEdGraphSchema_K2::PN_Then);
 auto* Children=G.Call(USceneComponent::StaticClass(),TEXT("GetChildrenComponents"));G.Link(G.Cast(G.Ret(Pick),USceneComponent::StaticClass()),G.P(Children,UEdGraphSchema_K2::PN_Self));G.Lit(Children,TEXT("bIncludeAllDescendants"),TEXT("true"));X=G.Run(X,Children);
 auto Array=[&](FName Function){FGraphNodeCreator<UK2Node_CallArrayFunction> C(*E);auto* N=C.CreateNode();N->SetFromFunction(UKismetArrayLibrary::StaticClass()->FindFunctionByName(Function));C.Finalize();G.Link(G.P(Children,TEXT("Children")),G.P(N,TEXT("TargetArray")));return N;};
 auto* Add=Array(TEXT("Array_AddUnique"));G.Link(G.Ret(Own),G.P(Add,TEXT("NewItem")));X=G.Run(X,Add);auto* Count=Array(TEXT("Array_Length"));auto Index=G.Get(TEXT("SunTargetIndex"));auto* Loop=G.Branch(Set(X,TEXT("SunTargetIndex"),G.Number(0)),Op(TEXT("Less_IntInt"),Index,G.Ret(Count)));auto Next=Join();
 FGraphNodeCreator<UK2Node_GetArrayItem> IC(*E);auto* Item=IC.CreateNode();IC.Finalize();G.Link(G.P(Children,TEXT("Children")),Item->GetTargetArrayPin());G.Link(Index,Item->GetIndexPin());auto* Candidate=G.Cast(Item->GetResultPin(),USkeletalMeshComponent::StaticClass());auto* Valid=G.Branch(G.P(Loop,UEdGraphSchema_K2::PN_Then),G.Valid(Candidate));G.Link(G.P(Valid,UEdGraphSchema_K2::PN_Else),Next.Key);X=Set(G.P(Valid,UEdGraphSchema_K2::PN_Then),TEXT("SunTarget"),Candidate);auto* Target=G.Get(TEXT("SunTarget"));
 auto SlotIndex=[&](int32 Slot){auto* N=G.Call(UMeshComponent::StaticClass(),TEXT("GetMaterialIndex"));G.Link(Target,G.P(N,UEdGraphSchema_K2::PN_Self));G.Lit(N,TEXT("MaterialSlotName"),Mesh->GetMaterials()[Slot].MaterialSlotName.ToString());return G.Ret(N);};
 int32 Body=INDEX_NONE,Hair=INDEX_NONE;for(int32 Slot:Slots){FString N=Mesh->GetMaterials()[Slot].MaterialSlotName.ToString();if(N.EndsWith(TEXT("Body")))Body=Slot;if(N.EndsWith(TEXT("Hair")))Hair=Slot;}if(Body<0||Hair<0)return TEXT("material fingerprint missing");
 auto* Ours=G.Branch(X,Op(TEXT("BooleanAND"),Op(TEXT("GreaterEqual_IntInt"),SlotIndex(Body),G.Number(0)),Op(TEXT("GreaterEqual_IntInt"),SlotIndex(Hair),G.Number(0))));G.Link(G.P(Ours,UEdGraphSchema_K2::PN_Else),Next.Key);X=G.P(Ours,UEdGraphSchema_K2::PN_Then);
 for(int32 I=0;I<Slots.Num();++I){
  auto* Effective=G.Get(IsEyeSlot(Slots[I])?TEXT("LeMalinEyeFinish"):TEXT("LeMalinReflectanceLevel"));
  auto Select=[&](double Threshold,UMaterialInterface* A,UMaterialInterface* B,UEdGraphPin* PinA=nullptr,UEdGraphPin* PinB=nullptr){auto* N=G.Call(UKismetMathLibrary::StaticClass(),TEXT("SelectObject"));if(PinA)G.Link(PinA,G.P(N,TEXT("A")));else G.Obj(N,TEXT("A"),A);if(PinB)G.Link(PinB,G.P(N,TEXT("B")));else G.Obj(N,TEXT("B"),B);G.Link(Op(TEXT("Less_DoubleDouble"),Effective,G.Number(Threshold)),G.P(N,TEXT("bSelectA")));return G.Ret(N);};
  UEdGraphPin* Chosen=nullptr;
  if(PlayerRelease)Chosen=IsEyeSlot(Slots[I])?Select(2,Assets[I][0],Assets[I][1]):Select(2.5,Assets[I][0],Assets[I][0]);
  else for(int32 Level=Assets[I].Num()-2;Level>=0;--Level)Chosen=Select(Level+.5,Assets[I][Level],Assets[I].Last(),nullptr,Chosen);
  if(!BlackAssets[I].IsEmpty()){
   UEdGraphPin* Black=nullptr;
   if(PlayerRelease)Black=Select(2.5,BlackAssets[I][0],BlackAssets[I][0]);
   else for(int32 Level=BlackAssets[I].Num()-2;Level>=0;--Level)Black=Select(Level+.5,BlackAssets[I][Level],BlackAssets[I].Last(),nullptr,Black);
   if(!PlayerRelease&&Mesh->GetMaterials()[Slots[I]].MaterialSlotName.ToString().EndsWith(TEXT("Cloth"))){
    UEdGraphPin* Thin=nullptr;for(int32 Level=SheerClothAssets.Num()-2;Level>=0;--Level)Thin=Select(Level+.5,SheerClothAssets[Level],SheerClothAssets.Last(),nullptr,Thin);
    auto* Stockings=G.Call(UKismetMathLibrary::StaticClass(),TEXT("SelectObject"));G.Link(Thin,G.P(Stockings,TEXT("A")));G.Link(Black,G.P(Stockings,TEXT("B")));
    G.Link(Op(TEXT("GreaterEqual_DoubleDouble"),G.Get(TEXT("LeMalinOutfitStyle")),G.Number(1.5)),G.P(Stockings,TEXT("bSelectA")));Black=G.Ret(Stockings);
    for(int32 Mode=0;Mode<StockingProbes.Num();++Mode){
     const auto& Row=StockingProbes[Mode];UEdGraphPin* Probe=nullptr;
     for(int32 Level=Row.Num()-2;Level>=0;--Level)Probe=Select(Level+.5,Row[Level],Row.Last(),nullptr,Probe);
     auto* Switch=G.Call(UKismetMathLibrary::StaticClass(),TEXT("SelectObject"));G.Link(Probe,G.P(Switch,TEXT("A")));G.Link(Black,G.P(Switch,TEXT("B")));
     G.Link(Op(TEXT("GreaterEqual_DoubleDouble"),G.Get(TEXT("LeMalinOutfitStyle")),G.Number(Mode+2.5)),G.P(Switch,TEXT("bSelectA")));Black=G.Ret(Switch);
    }
   }
   auto* Style=G.Call(UKismetMathLibrary::StaticClass(),TEXT("SelectObject"));G.Link(Black,G.P(Style,TEXT("A")));G.Link(Chosen,G.P(Style,TEXT("B")));
   G.Link(Op(TEXT("GreaterEqual_DoubleDouble"),G.Get(TEXT("LeMalinOutfitStyle")),G.Number(.5)),G.P(Style,TEXT("bSelectA")));Chosen=G.Ret(Style);
  }
  auto* Desired=G.Cast(Chosen,UMaterialInterface::StaticClass());auto* IndexPin=SlotIndex(Slots[I]);auto* Get=G.Call(UPrimitiveComponent::StaticClass(),TEXT("GetMaterial"));G.Link(Target,G.P(Get,UEdGraphSchema_K2::PN_Self));G.Link(IndexPin,G.P(Get,TEXT("ElementIndex")));
  auto* Apply=G.Branch(X,Op(TEXT("NotEqual_ObjectObject"),G.Ret(Get),Desired));auto Ready=Join();G.Link(G.P(Apply,UEdGraphSchema_K2::PN_Else),Ready.Key);auto* SetMat=G.Call(UPrimitiveComponent::StaticClass(),TEXT("SetMaterial"));G.Link(Target,G.P(SetMat,UEdGraphSchema_K2::PN_Self));G.Link(IndexPin,G.P(SetMat,TEXT("ElementIndex")));G.Link(Desired,G.P(SetMat,TEXT("Material")));G.Link(G.Run(G.P(Apply,UEdGraphSchema_K2::PN_Then),SetMat),Ready.Key);X=Ready.Value;
 }
 G.Link(X,Next.Key);G.Link(Set(Next.Value,TEXT("SunTargetIndex"),Op(TEXT("Add_IntInt"),Index,G.Number(1))),G.P(Loop,UEdGraphSchema_K2::PN_Execute));
 if(!G.OK)return G.Error;FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);BP->MarkPackageDirty();return BP->Status==BS_Error?TEXT("material comparison compile failed"):TEXT("OK: original / 50 / 25 / 12.5 / 6.25 percent linear albedo, plus black locator");
}
