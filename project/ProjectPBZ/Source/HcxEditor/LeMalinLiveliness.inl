#include "LeMalinBraidPhysics.inl"
#include "LeMalinEarPhysics.inl"
#include "LeMalinGaze.inl"
// Editor-only generator. Runtime assets use only engine-native nodes.
FString UHcxRetargetTools::BuildLeMalinLiveliness(UAnimBlueprint* BP, USkeletalMesh* Mesh)
{
 using namespace HcxPanel;
 if(!BP||!Mesh)return TEXT("missing lively assets");
 if(FindFProperty<FDoubleProperty>(BP->GeneratedClass,TEXT("BlinkClock")))return TEXT("already generated");
 FEdGraphPinType Number;Number.PinCategory=UEdGraphSchema_K2::PC_Real;Number.PinSubCategory=UEdGraphSchema_K2::PC_Double;
 FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("BlinkClock"),Number,TEXT("-3.5"));
 FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("BlinkProgress"),Number,TEXT("0"));
 FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("LeMalinExpressionMode"),Number,TEXT("4"));
 FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("LeMalinExpressionStrength"),Number,TEXT("1"));
 FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("FaceAutoClock"),Number,TEXT("-6"));
 FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("FaceAutoChoice"),Number,TEXT("1.5"));
 FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("FaceEffectiveMode"),Number,TEXT("0"));
 FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("LeMalinGazeMode"),Number,TEXT("1"));
 FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("LeMalinGazeStrength"),Number,TEXT("1"));
 FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("GazeClock"),Number,TEXT("-2.5"));
 for(const TCHAR* Name:{TEXT("GazeGoalH"),TEXT("GazeGoalV"),TEXT("GazeCurrentH"),TEXT("GazeCurrentV")})
  FBlueprintEditorUtils::AddMemberVariable(BP,Name,Number,TEXT("0"));
 for(const TCHAR* Name:{TEXT("FaceSleepyWeight"),TEXT("FaceSmileWeight"),TEXT("FaceFocusWeight")})
  FBlueprintEditorUtils::AddMemberVariable(BP,Name,Number,TEXT("0"));
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
 UEdGraph* EventGraph=BP->UbergraphPages[0];Graph G{BP,EventGraph};
 auto* Update=G.Event(UAnimInstance::StaticClass(),TEXT("BlueprintUpdateAnimation"));
 auto Math=[&](FName Function,UEdGraphPin* A,UEdGraphPin* B){auto* N=G.Call(UKismetMathLibrary::StaticClass(),Function);G.Link(A,G.P(N,TEXT("A")));G.Link(B,G.P(N,TEXT("B")));return G.Ret(N);};
 auto* Clock=G.Set(TEXT("BlinkClock"));G.Link(Math(TEXT("Add_DoubleDouble"),G.Get(TEXT("BlinkClock")),G.P(Update,TEXT("DeltaTimeX"))),G.P(Clock,TEXT("BlinkClock")));
 auto* Exec=G.Run(G.P(Update,UEdGraphSchema_K2::PN_Then),Clock);
 auto* Reset=G.Branch(Exec,Math(TEXT("GreaterEqual_DoubleDouble"),G.Get(TEXT("BlinkClock")),G.Number(.32)));
 auto* Random=G.Call(UKismetMathLibrary::StaticClass(),TEXT("RandomFloatInRange"));G.Lit(Random,TEXT("Min"),TEXT("3"));G.Lit(Random,TEXT("Max"),TEXT("6"));
 auto* ResetClock=G.Set(TEXT("BlinkClock"));G.Link(Math(TEXT("Multiply_DoubleDouble"),G.Ret(Random),G.Number(-1)),G.P(ResetClock,TEXT("BlinkClock")));
 auto* ResetExec=G.Run(G.P(Reset,UEdGraphSchema_K2::PN_Then),ResetClock);
 auto* Close=Math(TEXT("Divide_DoubleDouble"),G.Get(TEXT("BlinkClock")),G.Number(.10));
 auto* Open=Math(TEXT("Divide_DoubleDouble"),Math(TEXT("Subtract_DoubleDouble"),G.Number(.32),G.Get(TEXT("BlinkClock"))),G.Number(.18));
 auto* Clamp=G.Call(UKismetMathLibrary::StaticClass(),TEXT("FClamp"));G.Link(Math(TEXT("FMin"),Close,Open),G.P(Clamp,TEXT("Value")));G.Lit(Clamp,TEXT("Min"),TEXT("0"));G.Lit(Clamp,TEXT("Max"),TEXT("1"));
 auto* X=G.Ret(Clamp);auto* Smooth=Math(TEXT("Multiply_DoubleDouble"),Math(TEXT("Multiply_DoubleDouble"),X,X),Math(TEXT("Subtract_DoubleDouble"),G.Number(3),Math(TEXT("Multiply_DoubleDouble"),G.Number(2),X)));
 auto* Progress=G.Set(TEXT("BlinkProgress"));G.Link(Smooth,G.P(Progress,TEXT("BlinkProgress")));G.Link(ResetExec,G.P(Progress,UEdGraphSchema_K2::PN_Execute));
 Exec=G.Run(G.P(Reset,UEdGraphSchema_K2::PN_Else),Progress);
 auto* Morph=G.Call(UAnimInstance::StaticClass(),TEXT("SetMorphTarget"));G.Lit(Morph,TEXT("MorphTargetName"),TEXT("Blink"));G.Link(G.Get(TEXT("BlinkProgress")),G.P(Morph,TEXT("Value")));Exec=G.Run(Exec,Morph);
 auto* AutoClock=G.Set(TEXT("FaceAutoClock"));
 G.Link(Math(TEXT("Add_DoubleDouble"),G.Get(TEXT("FaceAutoClock")),G.P(Update,TEXT("DeltaTimeX"))),G.P(AutoClock,TEXT("FaceAutoClock")));Exec=G.Run(Exec,AutoClock);
 auto* AutoReset=G.Branch(Exec,Math(TEXT("GreaterEqual_DoubleDouble"),G.Get(TEXT("FaceAutoClock")),G.Number(0)));
 auto* Delay=G.Call(UKismetMathLibrary::StaticClass(),TEXT("RandomFloatInRange"));G.Lit(Delay,TEXT("Min"),TEXT("4"));G.Lit(Delay,TEXT("Max"),TEXT("8"));
 auto* ResetAutoClock=G.Set(TEXT("FaceAutoClock"));G.Link(Math(TEXT("Multiply_DoubleDouble"),G.Ret(Delay),G.Number(-1)),G.P(ResetAutoClock,TEXT("FaceAutoClock")));
 auto* Choice=G.Call(UKismetMathLibrary::StaticClass(),TEXT("RandomFloatInRange"));G.Lit(Choice,TEXT("Min"),TEXT("0"));G.Lit(Choice,TEXT("Max"),TEXT("3"));
 auto* StoreChoice=G.Set(TEXT("FaceAutoChoice"));G.Link(G.Ret(Choice),G.P(StoreChoice,TEXT("FaceAutoChoice")));
 auto* Changed=G.Run(G.Run(G.P(AutoReset,UEdGraphSchema_K2::PN_Then),ResetAutoClock),StoreChoice);
 auto SelectNumber=[&](UEdGraphPin* Condition,UEdGraphPin* First,UEdGraphPin* Second){
  auto* Node=G.Call(UKismetMathLibrary::StaticClass(),TEXT("SelectFloat"));
  G.Link(Condition,G.P(Node,TEXT("bPickA")));G.Link(First,G.P(Node,TEXT("A")));G.Link(Second,G.P(Node,TEXT("B")));return G.Ret(Node);
 };
 auto* Automatic=Math(TEXT("GreaterEqual_DoubleDouble"),G.Get(TEXT("LeMalinExpressionMode")),G.Number(3.5));
 auto* AutoPreset=SelectNumber(Math(TEXT("Less_DoubleDouble"),G.Get(TEXT("FaceAutoChoice")),G.Number(2)),G.Number(1),G.Number(2));
 AutoPreset=SelectNumber(Math(TEXT("Less_DoubleDouble"),G.Get(TEXT("FaceAutoChoice")),G.Number(1)),G.Number(0),AutoPreset);
 auto* Effective=G.Set(TEXT("FaceEffectiveMode"));G.Link(SelectNumber(Automatic,AutoPreset,G.Get(TEXT("LeMalinExpressionMode"))),G.P(Effective,TEXT("FaceEffectiveMode")));
 Exec=G.Run(Changed,Effective);G.Link(G.P(AutoReset,UEdGraphSchema_K2::PN_Else),G.P(Effective,UEdGraphSchema_K2::PN_Execute));
 const TCHAR* FaceNames[]={TEXT("FaceSleepy"),TEXT("FaceSmile"),TEXT("FaceFocus")};
 for(int32 Index=0;Index<3;++Index){
  FString Prefix=FaceNames[Index];FName WeightName(*(Prefix+TEXT("Weight")));
  auto* Selected=Math(TEXT("NearlyEqual_FloatFloat"),G.Get(TEXT("FaceEffectiveMode")),G.Number(Index+1));
  auto* Target=G.Call(UKismetMathLibrary::StaticClass(),TEXT("SelectFloat"));
  G.Link(G.Get(TEXT("LeMalinExpressionStrength")),G.P(Target,TEXT("A")));G.Lit(Target,TEXT("B"),TEXT("0"));G.Link(Selected,G.P(Target,TEXT("bPickA")));
  auto* Interp=G.Call(UKismetMathLibrary::StaticClass(),TEXT("FInterpTo"));
  G.Link(G.Get(WeightName),G.P(Interp,TEXT("Current")));G.Link(G.Ret(Target),G.P(Interp,TEXT("Target")));
  G.Link(G.P(Update,TEXT("DeltaTimeX")),G.P(Interp,TEXT("DeltaTime")));G.Link(SelectNumber(Automatic,G.Number(1.8),G.Number(8)),G.P(Interp,TEXT("InterpSpeed")));
  auto* Store=G.Set(WeightName);G.Link(G.Ret(Interp),G.P(Store,WeightName));Exec=G.Run(Exec,Store);
  for(bool Eyes:{false,true}){
   auto* Apply=G.Call(UAnimInstance::StaticClass(),TEXT("SetMorphTarget"));
   G.Lit(Apply,TEXT("MorphTargetName"),Prefix+(Eyes?TEXT("Eyes"):TEXT("Face")));
   auto* Value=G.Get(WeightName);
   if(Eyes)Value=Math(TEXT("Multiply_DoubleDouble"),Value,Math(TEXT("Subtract_DoubleDouble"),G.Number(1),G.Get(TEXT("BlinkProgress"))));
   G.Link(Value,G.P(Apply,TEXT("Value")));Exec=G.Run(Exec,Apply);
  }
 }
 Exec=BuildLeMalinGazeGraph(G,Update,Exec);
 if(!G.OK)return G.Error;
 UEdGraph* AnimGraph=nullptr;UAnimGraphNode_Root* Root=nullptr;TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
 for(auto* Page:Graphs)for(UEdGraphNode* Node:Page->Nodes)if(auto* R=Cast<UAnimGraphNode_Root>(Node)){AnimGraph=Page;Root=R;}
 if(!Root)return TEXT("animation root missing");Graph A{BP,AnimGraph};auto* Input=A.P(Root,TEXT("Result"));if(Input->LinkedTo.Num()!=1)return TEXT("animation input missing");
 auto* Current=Input->LinkedTo[0];Input->BreakAllPinLinks();auto Out=[](UEdGraphNode* N){for(auto* P:N->Pins)if(P->Direction==EGPD_Output)return P;return (UEdGraphPin*)nullptr;};
 FGraphNodeCreator<UAnimGraphNode_LocalToComponentSpace> LC(*AnimGraph);auto* Local=LC.CreateNode();LC.Finalize();A.Link(Current,A.P(Local,TEXT("LocalPose")));Current=Out(Local);
 auto* Physics=BuildLeMalinBraidPhysics(Mesh);if(!Physics)return TEXT("braid physics asset generation failed");
 auto* Ears=BuildLeMalinEarPhysics(Mesh);if(!Ears)return TEXT("ear physics asset generation failed");
 FGraphNodeCreator<UAnimGraphNode_RigidBody> EC(*AnimGraph);auto* EarNode=EC.CreateNode();
 EarNode->Node.OverridePhysicsAsset=Ears;EarNode->Node.bDefaultToSkeletalMeshPhysicsAsset=false;
 EarNode->Node.bOverrideWorldGravity=true;EarNode->Node.OverrideWorldGravity=FVector(0,0,-980);
 EarNode->Node.SimulationSpace=ESimulationSpace::ComponentSpace;EarNode->Node.SimulationTiming=ESimulationTiming::Synchronous;
 EarNode->Node.SimSpaceSettings.WorldAlpha=.65f;EarNode->Node.SimSpaceSettings.VelocityScaleZ=1.f;
 EarNode->Node.SimSpaceSettings.DampingAlpha=0.f;EarNode->Node.SimSpaceSettings.MaxLinearVelocity=1000;
 EarNode->Node.SimSpaceSettings.MaxAngularVelocity=3;EarNode->Node.SimSpaceSettings.MaxLinearAcceleration=1800;
 EarNode->Node.SimSpaceSettings.MaxAngularAcceleration=18;EarNode->Node.SimSpaceSettings.ExternalLinearDragV=FVector(.025,.025,0);
 EarNode->Node.ComponentLinearAccScale=FVector::ZeroVector;EarNode->Node.ComponentLinearVelScale=FVector::ZeroVector;
 EarNode->Node.bEnableWorldGeometry=false;EarNode->Node.bUseExternalClothCollision=false;
 EarNode->Node.bForceDisableCollisionBetweenConstraintBodies=true;EarNode->Node.bClampLinearTranslationLimitToRefPose=true;
 EarNode->Node.bFreezeIncomingPoseOnStart=false;EarNode->Node.bTransferBoneVelocities=false;
 EarNode->Node.bUseLocalLODThresholdOnly=true;EarNode->Node.LODThreshold=-1;EarNode->Node.EvaluationResetTime=.5f;
 EarNode->SetTag(TEXT("LeMalinEarDynamics"));EC.Finalize();A.Link(Current,A.P(EarNode,TEXT("ComponentPose")));Current=Out(EarNode);
 FGraphNodeCreator<UAnimGraphNode_RigidBody> RB(*AnimGraph);auto* Rigid=RB.CreateNode();
 Rigid->Node.OverridePhysicsAsset=Physics;Rigid->Node.bDefaultToSkeletalMeshPhysicsAsset=false;
 Rigid->Node.bOverrideWorldGravity=true;Rigid->Node.OverrideWorldGravity=FVector(0,0,-980);
 Rigid->Node.SimulationSpace=ESimulationSpace::ComponentSpace;Rigid->Node.SimulationTiming=ESimulationTiming::Synchronous;
 Rigid->Node.SimSpaceSettings.WorldAlpha=.70f;Rigid->Node.SimSpaceSettings.VelocityScaleZ=1.f;
 Rigid->Node.SimSpaceSettings.DampingAlpha=0.f;Rigid->Node.SimSpaceSettings.MaxLinearVelocity=1200;
 Rigid->Node.SimSpaceSettings.MaxAngularVelocity=0;Rigid->Node.SimSpaceSettings.MaxLinearAcceleration=2200;
 Rigid->Node.SimSpaceSettings.MaxAngularAcceleration=0;Rigid->Node.SimSpaceSettings.ExternalLinearDragV=FVector(.08,.08,0);
 Rigid->Node.ComponentLinearAccScale=FVector::ZeroVector;Rigid->Node.ComponentLinearVelScale=FVector::ZeroVector;
 Rigid->Node.bEnableWorldGeometry=false;Rigid->Node.bUseExternalClothCollision=false;
 Rigid->Node.bForceDisableCollisionBetweenConstraintBodies=true;Rigid->Node.bClampLinearTranslationLimitToRefPose=true;
 Rigid->Node.bFreezeIncomingPoseOnStart=false;Rigid->Node.bTransferBoneVelocities=false;
 Rigid->Node.bUseLocalLODThresholdOnly=true;Rigid->Node.LODThreshold=-1;Rigid->Node.EvaluationResetTime=.5f;
 RB.Finalize();A.Link(Current,A.P(Rigid,TEXT("ComponentPose")));Current=Out(Rigid);
 FGraphNodeCreator<UAnimGraphNode_ComponentToLocalSpace> CL(*AnimGraph);auto* Component=CL.CreateNode();CL.Finalize();A.Link(Current,A.P(Component,TEXT("ComponentPose")));A.Link(Out(Component),Input);
 if(!A.OK)return A.Error;FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);BP->MarkPackageDirty();
 return BP->Status==BS_Error?TEXT("liveliness compile failed"):TEXT("OK: blink, independent root-to-tip ear physics, accepted braid/body collision");
}
