#include "HcxRetargetTools.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_RetargetPoseFromMesh.h"
#include "AnimGraphNode_BlendBoneByChannel.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Event.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/NavMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/UnrealType.h"

FString UHcxRetargetTools::BuildGaitGraph(UAnimBlueprint* BP,UIKRetargeter* Base,UIKRetargeter* Left,UIKRetargeter* Right) {
 if(!BP||!Base||!Left||!Right)return TEXT("missing assets");
 FEdGraphPinType FloatType;FloatType.PinCategory=UEdGraphSchema_K2::PC_Real;FloatType.PinSubCategory=UEdGraphSchema_K2::PC_Float;
 for(FName Name:{FName(TEXT("HcxGaitAlphaL")),FName(TEXT("HcxGaitAlphaR"))}) {
  bool Exists=false;for(const auto& V:BP->NewVariables)Exists|=V.VarName==Name;
  if(!Exists)FBlueprintEditorUtils::AddMemberVariable(BP,Name,FloatType,TEXT("0.0"));
 }
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
 bool OK=true;FString Error;
 auto P=[&](UEdGraphNode* N,FName Name)->UEdGraphPin* {
  auto* Pin=N?N->FindPin(Name):nullptr;
  if(!Pin){OK=false;Error+=FString::Printf(TEXT("missing pin %s on %s; "),*Name.ToString(),N?*N->GetClass()->GetName():TEXT("null"));}return Pin;
 };
 auto Link=[&](UEdGraphPin* A,UEdGraphPin* B){if(!A||!B||!A->GetOwningNode()->GetGraph()->GetSchema()->TryCreateConnection(A,B)){OK=false;Error+=TEXT("connection failed; ");}};
 auto Call=[&](UEdGraph* G,UClass* Class,FName Name)->UK2Node_CallFunction* {
  auto* F=Class->FindFunctionByName(Name);if(!F){OK=false;Error+=TEXT("missing function ")+Name.ToString()+TEXT("; ");return nullptr;}
  FGraphNodeCreator<UK2Node_CallFunction> C(*G);auto* N=C.CreateNode();N->SetFromFunction(F);C.Finalize();return N;
 };
 auto Get=[&](UEdGraph* G,FName Name,UClass* Class,UEdGraphPin* Object)->UEdGraphPin* {
  FGraphNodeCreator<UK2Node_VariableGet> C(*G);auto* N=C.CreateNode();
  if(Class)N->VariableReference.SetExternalMember(Name,Class);else N->VariableReference.SetSelfMember(Name);
  C.Finalize();if(Object)Link(Object,P(N,UEdGraphSchema_K2::PN_Self));return P(N,Name);
 };
 auto Set=[&](UEdGraph* G,FName Name)->UK2Node_VariableSet* {
  FGraphNodeCreator<UK2Node_VariableSet> C(*G);auto* N=C.CreateNode();N->VariableReference.SetSelfMember(Name);C.Finalize();return N;
 };
 auto Literal=[&](UEdGraphNode* N,FName Name,const TCHAR* Value){if(auto* Pin=P(N,Name))Pin->DefaultValue=Value;};
 UEdGraph* EventGraph=BP->UbergraphPages.Num()?BP->UbergraphPages[0]:nullptr;
 if(!EventGraph)return TEXT("missing event graph");
 for(auto* N:TArray<UEdGraphNode*>(EventGraph->Nodes))FBlueprintEditorUtils::RemoveNode(BP,N,true);
 int32 Y=0;auto* Event=FKismetEditorUtilities::AddDefaultEventNode(BP,EventGraph,TEXT("BlueprintUpdateAnimation"),UAnimInstance::StaticClass(),Y);
 auto* Pawn=Call(EventGraph,UAnimInstance::StaticClass(),TEXT("TryGetPawnOwner"));
 FGraphNodeCreator<UK2Node_DynamicCast> CastCreator(*EventGraph);auto* Cast=CastCreator.CreateNode();Cast->TargetType=ACharacter::StaticClass();Cast->SetPurity(false);CastCreator.Finalize();
 Link(P(Event,UEdGraphSchema_K2::PN_Then),P(Cast,UEdGraphSchema_K2::PN_Execute));Link(P(Pawn,UEdGraphSchema_K2::PN_ReturnValue),Cast->GetCastSourcePin());
 auto* Character=Cast->GetCastResultPin();auto* Mesh=Get(EventGraph,TEXT("Mesh"),ACharacter::StaticClass(),Character);auto* Movement=Get(EventGraph,TEXT("CharacterMovement"),ACharacter::StaticClass(),Character);
 auto* Anim=Call(EventGraph,USkeletalMeshComponent::StaticClass(),TEXT("GetAnimInstance"));Link(Mesh,P(Anim,UEdGraphSchema_K2::PN_Self));auto* AnimOut=P(Anim,UEdGraphSchema_K2::PN_ReturnValue);
 auto* Valid=Call(EventGraph,UKismetSystemLibrary::StaticClass(),TEXT("IsValid"));Link(AnimOut,P(Valid,TEXT("Object")));
 FGraphNodeCreator<UK2Node_IfThenElse> ValidCreator(*EventGraph);auto* ValidBranch=ValidCreator.CreateNode();ValidCreator.Finalize();
 Link(Cast->GetValidCastPin(),P(ValidBranch,UEdGraphSchema_K2::PN_Execute));Link(P(Valid,UEdGraphSchema_K2::PN_ReturnValue),P(ValidBranch,UEdGraphSchema_K2::PN_Condition));
 auto* Montage=Call(EventGraph,UAnimInstance::StaticClass(),TEXT("IsAnyMontagePlaying"));Link(AnimOut,P(Montage,UEdGraphSchema_K2::PN_Self));
 auto* Ground=Call(EventGraph,UNavMovementComponent::StaticClass(),TEXT("IsMovingOnGround"));Link(Movement,P(Ground,UEdGraphSchema_K2::PN_Self));
 auto* Crouch=Call(EventGraph,UNavMovementComponent::StaticClass(),TEXT("IsCrouching"));Link(Movement,P(Crouch,UEdGraphSchema_K2::PN_Self));
 auto Not=[&](UEdGraphPin* Input){auto* N=Call(EventGraph,UKismetMathLibrary::StaticClass(),TEXT("Not_PreBool"));Link(Input,P(N,TEXT("A")));return P(N,UEdGraphSchema_K2::PN_ReturnValue);};
 auto And=[&](UEdGraphPin* A,UEdGraphPin* B){auto* N=Call(EventGraph,UKismetMathLibrary::StaticClass(),TEXT("BooleanAND"));Link(A,P(N,TEXT("A")));Link(B,P(N,TEXT("B")));return P(N,UEdGraphSchema_K2::PN_ReturnValue);};
 auto Greater=[&](UEdGraphPin* A,const TCHAR* B){auto* N=Call(EventGraph,UKismetMathLibrary::StaticClass(),TEXT("Greater_DoubleDouble"));Link(A,P(N,TEXT("A")));Literal(N,TEXT("B"),B);return P(N,UEdGraphSchema_K2::PN_ReturnValue);};
 auto* Velocity=Call(EventGraph,AActor::StaticClass(),TEXT("GetVelocity"));Link(Character,P(Velocity,UEdGraphSchema_K2::PN_Self));auto* Speed=Call(EventGraph,UKismetMathLibrary::StaticClass(),TEXT("VSize"));Link(P(Velocity,UEdGraphSchema_K2::PN_ReturnValue),P(Speed,TEXT("A")));
 auto* Curve=Call(EventGraph,UAnimInstance::StaticClass(),TEXT("GetCurveValue"));Link(AnimOut,P(Curve,UEdGraphSchema_K2::PN_Self));Literal(Curve,TEXT("CurveName"),TEXT("movedata_speed"));
 auto* Enabled=And(And(P(Ground,UEdGraphSchema_K2::PN_ReturnValue),Not(P(Montage,UEdGraphSchema_K2::PN_ReturnValue))),And(Not(P(Crouch,UEdGraphSchema_K2::PN_ReturnValue)),And(Greater(P(Speed,UEdGraphSchema_K2::PN_ReturnValue),TEXT("30.0")),Greater(P(Curve,UEdGraphSchema_K2::PN_ReturnValue),TEXT("10.0")))));
 FGraphNodeCreator<UK2Node_IfThenElse> GateCreator(*EventGraph);auto* Gate=GateCreator.CreateNode();GateCreator.Finalize();Link(P(ValidBranch,UEdGraphSchema_K2::PN_Then),P(Gate,UEdGraphSchema_K2::PN_Execute));Link(Enabled,P(Gate,UEdGraphSchema_K2::PN_Condition));
 auto* ZeroL=Set(EventGraph,TEXT("HcxGaitAlphaL"));auto* ZeroR=Set(EventGraph,TEXT("HcxGaitAlphaR"));Literal(ZeroL,TEXT("HcxGaitAlphaL"),TEXT("0.0"));Literal(ZeroR,TEXT("HcxGaitAlphaR"),TEXT("0.0"));
 Link(Cast->GetInvalidCastPin(),P(ZeroL,UEdGraphSchema_K2::PN_Execute));Link(P(ValidBranch,UEdGraphSchema_K2::PN_Else),P(ZeroL,UEdGraphSchema_K2::PN_Execute));Link(P(Gate,UEdGraphSchema_K2::PN_Else),P(ZeroL,UEdGraphSchema_K2::PN_Execute));Link(P(ZeroL,UEdGraphSchema_K2::PN_Then),P(ZeroR,UEdGraphSchema_K2::PN_Execute));
 auto Socket=[&](FName Function,FName Bone){auto* N=Call(EventGraph,USceneComponent::StaticClass(),Function);Link(Mesh,P(N,UEdGraphSchema_K2::PN_Self));if(auto* Pin=P(N,TEXT("InSocketName")))Pin->DefaultValue=Bone.ToString();return P(N,UEdGraphSchema_K2::PN_ReturnValue);};
 auto* Up=Call(EventGraph,UKismetMathLibrary::StaticClass(),TEXT("GetUpVector"));Link(Socket(TEXT("GetSocketRotation"),TEXT("pelvis")),P(Up,TEXT("InRot")));
 UEdGraphPin* Next=P(Gate,UEdGraphSchema_K2::PN_Then);
 for(bool IsLeft:{true,false}) {
  const FString S=IsLeft?TEXT("l"):TEXT("r");const FName Variable=IsLeft?TEXT("HcxGaitAlphaL"):TEXT("HcxGaitAlphaR");
  auto* Sum=Call(EventGraph,UKismetMathLibrary::StaticClass(),TEXT("Add_VectorVector"));Link(Socket(TEXT("GetSocketLocation"),FName(TEXT("thigh_")+S)),P(Sum,TEXT("A")));Link(Socket(TEXT("GetSocketLocation"),FName(TEXT("foot_")+S)),P(Sum,TEXT("B")));
  auto* Half=Call(EventGraph,UKismetMathLibrary::StaticClass(),TEXT("Multiply_VectorFloat"));Link(P(Sum,UEdGraphSchema_K2::PN_ReturnValue),P(Half,TEXT("A")));Literal(Half,TEXT("B"),TEXT("0.5"));
  auto* Offset=Call(EventGraph,UKismetMathLibrary::StaticClass(),TEXT("Subtract_VectorVector"));Link(Socket(TEXT("GetSocketLocation"),FName(TEXT("calf_")+S)),P(Offset,TEXT("A")));Link(P(Half,UEdGraphSchema_K2::PN_ReturnValue),P(Offset,TEXT("B")));
  auto* Dot=Call(EventGraph,UKismetMathLibrary::StaticClass(),TEXT("Dot_VectorVector"));Link(P(Offset,UEdGraphSchema_K2::PN_ReturnValue),P(Dot,TEXT("A")));Link(P(Up,UEdGraphSchema_K2::PN_ReturnValue),P(Dot,TEXT("B")));
  auto* Sign=Call(EventGraph,UKismetMathLibrary::StaticClass(),TEXT("Multiply_DoubleDouble"));Link(P(Dot,UEdGraphSchema_K2::PN_ReturnValue),P(Sign,TEXT("A")));Literal(Sign,TEXT("B"),IsLeft?TEXT("-1.0"):TEXT("1.0"));
  auto* Margin=Call(EventGraph,UKismetMathLibrary::StaticClass(),TEXT("Subtract_DoubleDouble"));Link(P(Sign,UEdGraphSchema_K2::PN_ReturnValue),P(Margin,TEXT("A")));Literal(Margin,TEXT("B"),TEXT("2.0"));
  auto* Range=Call(EventGraph,UKismetMathLibrary::StaticClass(),TEXT("Divide_DoubleDouble"));Link(P(Margin,UEdGraphSchema_K2::PN_ReturnValue),P(Range,TEXT("A")));Literal(Range,TEXT("B"),TEXT("6.0"));
  auto* Clamp=Call(EventGraph,UKismetMathLibrary::StaticClass(),TEXT("FClamp"));Link(P(Range,UEdGraphSchema_K2::PN_ReturnValue),P(Clamp,TEXT("Value")));Literal(Clamp,TEXT("Min"),TEXT("0.0"));Literal(Clamp,TEXT("Max"),TEXT("1.0"));
  auto* Smooth=Call(EventGraph,UKismetMathLibrary::StaticClass(),TEXT("FInterpTo"));Link(Get(EventGraph,Variable,nullptr,nullptr),P(Smooth,TEXT("Current")));Link(P(Clamp,UEdGraphSchema_K2::PN_ReturnValue),P(Smooth,TEXT("Target")));Link(P(Event,TEXT("DeltaTimeX")),P(Smooth,TEXT("DeltaTime")));Literal(Smooth,TEXT("InterpSpeed"),TEXT("12.0"));
  auto* Store=Set(EventGraph,Variable);Link(Next,P(Store,UEdGraphSchema_K2::PN_Execute));Link(P(Smooth,UEdGraphSchema_K2::PN_ReturnValue),P(Store,Variable));Next=P(Store,UEdGraphSchema_K2::PN_Then);
 }
 UEdGraph* Graph=nullptr;UAnimGraphNode_Root* Root=nullptr;TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
 for(auto* G:Graphs)for(UEdGraphNode* N:G->Nodes)if(auto* R=::Cast<UAnimGraphNode_Root>(N)){Graph=G;Root=R;break;}
 if(!Graph||!Root)return TEXT("missing animation root");
 for(auto* N:TArray<UEdGraphNode*>(Graph->Nodes))if(N!=Root)FBlueprintEditorUtils::RemoveNode(BP,N,true);
 auto Out=[&](UEdGraphNode* N)->UEdGraphPin* {for(auto* Pin:N->Pins)if(Pin->Direction==EGPD_Output)return Pin;return nullptr;};
 auto Retarget=[&](UIKRetargeter* Asset){FGraphNodeCreator<UAnimGraphNode_RetargetPoseFromMesh> C(*Graph);auto* N=C.CreateNode();auto* Prop=FindFProperty<FStructProperty>(N->GetClass(),TEXT("Node"));auto* V=Prop->ContainerPtrToValuePtr<FAnimNode_RetargetPoseFromMesh>(N);V->IKRetargeterAsset=Asset;V->RetargetFrom=ERetargetSourceMode::ParentSkeletalMeshComponent;C.Finalize();return Out(N);};
 auto* Current=Retarget(Base);
 for(bool IsLeft:{true,false}) {
  FGraphNodeCreator<UAnimGraphNode_BlendBoneByChannel> C(*Graph);auto* N=C.CreateNode();N->BlendNode.TransformsSpace=BCS_ComponentSpace;N->BlendNode.Alpha=0;
  for(const TCHAR* Part:{TEXT("Thigh"),TEXT("Calf"),TEXT("Foot"),TEXT("ThighRoll"),TEXT("Knee")}) {
   FName Bone;
   if(FCString::Strcmp(Part,TEXT("ThighRoll"))==0)Bone=IsLeft?TEXT("RB_L_ThighRoll"):TEXT("RB_R_ThighRoll");
   else if(FCString::Strcmp(Part,TEXT("Knee"))==0)Bone=IsLeft?TEXT("MH_L_Knee"):TEXT("MH_R_Knee");
   else Bone=FName(FString(IsLeft?TEXT("SKEL_L_"):TEXT("SKEL_R_"))+Part);
   FBlendBoneByChannelEntry E;E.SourceBone.BoneName=Bone;E.TargetBone.BoneName=Bone;E.bBlendScale=false;N->BlendNode.BoneDefinitions.Add(E);
  }
  C.Finalize();Link(Current,P(N,TEXT("A")));Link(Retarget(IsLeft?Left:Right),P(N,TEXT("B")));Link(Get(Graph,IsLeft?TEXT("HcxGaitAlphaL"):TEXT("HcxGaitAlphaR"),nullptr,nullptr),P(N,TEXT("Alpha")));Current=Out(N);
 }
 Link(Current,P(Root,TEXT("Result")));
 if(!OK)return Error;
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);BP->MarkPackageDirty();
 return BP->Status==BS_Error?TEXT("blueprint compilation error"):TEXT("OK");
}

FString UHcxRetargetTools::BuildMovementLegGraph(UAnimBlueprint* BP,UIKRetargeter* Baseline,UIKRetargeter* Direct){
 FString Result=BuildGaitGraph(BP,Baseline,Direct,Direct);if(Result!=TEXT("OK"))return Result;
 int32 Changed=0;
 for(UEdGraph* Graph:BP->UbergraphPages)for(UEdGraphNode* Node:Graph->Nodes)if(auto* Call=Cast<UK2Node_CallFunction>(Node))if(Call->FunctionReference.GetMemberName()==TEXT("FInterpTo")){
  auto* Current=Call->FindPin(TEXT("Current"));if(!Current||Current->LinkedTo.IsEmpty())continue;
  auto* Variable=Cast<UK2Node_VariableGet>(Current->LinkedTo[0]->GetOwningNode());if(!Variable)continue;
  const FName Name=Variable->VariableReference.GetMemberName();if(Name!=TEXT("HcxGaitAlphaL")&&Name!=TEXT("HcxGaitAlphaR"))continue;
  auto* Target=Call->FindPin(TEXT("Target"));Target->BreakAllPinLinks();Target->DefaultValue=TEXT("1.0");++Changed;
 }
 if(Changed!=2)return TEXT("movement blend targets missing");
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);BP->MarkPackageDirty();return BP->Status==BS_Error?TEXT("movement direct-leg graph compilation failed"):TEXT("OK");
}
