#include "HcxRetargetTools.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_TwoBoneIK.h"
#include "AnimGraphNode_ModifyBone.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialParameterCollection.h"
#include "KismetAnimationLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/NavMovementComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

FString UHcxRetargetTools::ScaleGaitReferenceVectors(UAnimBlueprint* BP,double Ratio){
 if(!BP||Ratio<=0)return TEXT("invalid gait scale");TArray<UK2Node_CallFunction*> Vectors;
 for(UEdGraph* E:BP->UbergraphPages)for(UEdGraphNode* N:E->Nodes)if(auto* F=Cast<UK2Node_CallFunction>(N))if(F->FunctionReference.GetMemberName()==TEXT("MakeVector")){
  auto* X=F->FindPin(TEXT("X"));auto* Y=F->FindPin(TEXT("Y"));auto* Z=F->FindPin(TEXT("Z"));if(!X||!Y||!Z||!X->LinkedTo.IsEmpty()||!Y->LinkedTo.IsEmpty()||!Z->LinkedTo.IsEmpty())continue;
  const FVector V(FCString::Atod(*X->DefaultValue),FCString::Atod(*Y->DefaultValue),FCString::Atod(*Z->DefaultValue));if(V.Size()>40&&V.Size()<60&&FMath::Abs(V.Y)<3&&FMath::Abs(V.Z)<3)Vectors.Add(F);
 }
 if(Vectors.Num()!=4)return FString::Printf(TEXT("expected four leg reference vectors; got %d"),Vectors.Num());
 for(auto* N:Vectors)for(FName Axis:{FName(TEXT("X")),FName(TEXT("Y")),FName(TEXT("Z"))}){auto* P=N->FindPin(Axis);P->DefaultValue=FString::Printf(TEXT("%.12f"),FCString::Atod(*P->DefaultValue)*Ratio);}
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);BP->MarkPackageDirty();return BP->Status==BS_Error?TEXT("scaled gait compilation failed"):TEXT("OK");
}

FString UHcxRetargetTools::BuildWidthGraph(UAnimBlueprint* BP,const FString& SettingsJson){
 TSharedPtr<FJsonObject> Settings;if(!BP||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(SettingsJson),Settings))return TEXT("width settings missing");
 FEdGraphPinType FloatType;FloatType.PinCategory=UEdGraphSchema_K2::PC_Real;FloatType.PinSubCategory=UEdGraphSchema_K2::PC_Float;
 FEdGraphPinType VectorType;VectorType.PinCategory=UEdGraphSchema_K2::PC_Struct;VectorType.PinSubCategoryObject=TBaseStructure<FVector>::Get();FEdGraphPinType RotatorType=VectorType;RotatorType.PinSubCategoryObject=TBaseStructure<FRotator>::Get();
 FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("HcxWidthWeight"),FloatType,TEXT("0.0"));for(FName Name:{FName(TEXT("HcxWidthOffsetL")),FName(TEXT("HcxWidthOffsetR")),FName(TEXT("HcxWidthPoleL")),FName(TEXT("HcxWidthPoleR"))})FBlueprintEditorUtils::AddMemberVariable(BP,Name,VectorType);for(FName Name:{FName(TEXT("HcxWidthRollL")),FName(TEXT("HcxWidthRollR"))})FBlueprintEditorUtils::AddMemberVariable(BP,Name,RotatorType);
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);bool OK=true;FString Error;UEdGraph* E=BP->UbergraphPages[0];
 bool ForceProbe=false;Settings->TryGetBoolField(TEXT("force_visible_probe"),ForceProbe);
 bool ParentMovement=false;Settings->TryGetBoolField(TEXT("parent_movement_gate"),ParentMovement);double KneeInward=0;Settings->TryGetNumberField(TEXT("knee_pole_inward_cm"),KneeInward);
 double ToeOut=0;Settings->TryGetNumberField(TEXT("toe_out_degrees"),ToeOut);
 bool FootYawProbe=false;Settings->TryGetBoolField(TEXT("foot_yaw_force_probe"),FootYawProbe);
 double BackFollow=0;Settings->TryGetNumberField(TEXT("backswing_foot_follow_max_degrees"),BackFollow);
 double FootUntwist=0;Settings->TryGetNumberField(TEXT("foot_inward_twist_max_degrees"),FootUntwist);
 bool FootTwistLock=false;Settings->TryGetBoolField(TEXT("foot_relative_twist_lock"),FootTwistLock);
 bool FootPanel=false;Settings->TryGetBoolField(TEXT("foot_runtime_panel"),FootPanel);
 if(FootPanel){for(auto Pair:{TPair<FName,const TCHAR*>(TEXT("HcxWidthCm"),TEXT("3.0")),TPair<FName,const TCHAR*>(TEXT("HcxKneeCm"),TEXT("6.0")),TPair<FName,const TCHAR*>(TEXT("HcxBlendSeconds"),TEXT("0.35")),TPair<FName,const TCHAR*>(TEXT("HcxWidthTarget"),TEXT("0.0"))})FBlueprintEditorUtils::AddMemberVariable(BP,Pair.Key,FloatType,Pair.Value);}
 if(FootPanel)for(FName Name:{FName(TEXT("HcxIdleEnabled")),FName(TEXT("HcxIdleFootEnabled")),FName(TEXT("HcxIdleWidthCm")),FName(TEXT("HcxIdleKneeCm")),FName(TEXT("HcxIdleAngle")),FName(TEXT("HcxIdleTarget")),FName(TEXT("HcxIdleWeight")),FName(TEXT("HcxLegWeight")),FName(TEXT("HcxEffectiveWidth")),FName(TEXT("HcxEffectiveKnee")),FName(TEXT("HcxEffectiveAngleL")),FName(TEXT("HcxEffectiveAngleR")),FName(TEXT("HcxFootNormalWeight"))})FBlueprintEditorUtils::AddMemberVariable(BP,Name,FloatType,TEXT("0.0"));
 if(FootPanel){for(FName Name:{FName(TEXT("HcxFootRequestedL")),FName(TEXT("HcxFootRequestedR"))})FBlueprintEditorUtils::AddMemberVariable(BP,Name,FloatType,TEXT("17.0"));FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("HcxFootPreview"),FloatType,TEXT("0.0"));}
 if(FootPanel){FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("HcxFootEnabled"),FloatType,TEXT("1.0"));for(FName Name:{FName(TEXT("HcxFootDebugSpeed")),FName(TEXT("HcxFootDebugGround")),FName(TEXT("HcxFootDebugMontage"))})FBlueprintEditorUtils::AddMemberVariable(BP,Name,FloatType,TEXT("0.0"));for(FName Name:{FName(TEXT("HcxFootUsedL")),FName(TEXT("HcxFootUsedR"))})FBlueprintEditorUtils::AddMemberVariable(BP,Name,FloatType,TEXT("-999.0"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);}
 if(FootPanel)for(auto& V:BP->NewVariables){if(V.VarName==TEXT("HcxWidthCm"))V.DefaultValue=TEXT("3.0");else if(V.VarName==TEXT("HcxKneeCm"))V.DefaultValue=TEXT("6.0");else if(V.VarName==TEXT("HcxFootRequestedL")||V.VarName==TEXT("HcxFootRequestedR"))V.DefaultValue=TEXT("17.0");else if(V.VarName==TEXT("HcxBlendSeconds"))V.DefaultValue=TEXT("0.35");else if(V.VarName==TEXT("HcxIdleEnabled"))V.DefaultValue=TEXT("1.0");else if(V.VarName==TEXT("HcxIdleWidthCm"))V.DefaultValue=TEXT("-4.08");else if(V.VarName==TEXT("HcxIdleKneeCm"))V.DefaultValue=TEXT("3.99");else if(V.VarName==TEXT("HcxIdleAngle")||V.VarName==TEXT("HcxIdleFootEnabled"))V.DefaultValue=TEXT("0.0");}
 USkeletalMesh* TwistReference=(FootUntwist>0||FootTwistLock)?LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Hcx2B/SK_2B.SK_2B")):nullptr;
 if((FootUntwist>0||FootTwistLock)&&!TwistReference)return TEXT("foot twist reference missing");
 if(BackFollow>0||FootUntwist>0||FootTwistLock){for(FName Name:{FName(TEXT("HcxFootBackL")),FName(TEXT("HcxFootBackR"))})FBlueprintEditorUtils::AddMemberVariable(BP,Name,FloatType,TEXT("0.0"));for(FName Name:{FName(TEXT("HcxFootFollowL")),FName(TEXT("HcxFootFollowR"))})FBlueprintEditorUtils::AddMemberVariable(BP,Name,RotatorType);FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);}
 if(ForceProbe)for(UEdGraphNode* N:E->Nodes)if(auto* Branch=Cast<UK2Node_IfThenElse>(N))if(auto* Then=Branch->FindPin(UEdGraphSchema_K2::PN_Then))for(auto* Link:Then->LinkedTo)if(auto* Variable=Cast<UK2Node_VariableSet>(Link->GetOwningNode()))if(Variable->VariableReference.GetMemberName()==TEXT("HcxGaitAlphaL")){auto* Condition=Branch->FindPin(UEdGraphSchema_K2::PN_Condition);Condition->BreakAllPinLinks();Condition->DefaultValue=TEXT("true");}
 auto P=[&](UEdGraphNode* N,FName Name)->UEdGraphPin*{auto* Pin=N?N->FindPin(Name):nullptr;if(!Pin){OK=false;Error+=TEXT("missing ")+Name.ToString()+TEXT("; ");}return Pin;};auto Link=[&](UEdGraphPin* A,UEdGraphPin* B){if(!A||!B||!A->GetOwningNode()->GetGraph()->GetSchema()->TryCreateConnection(A,B)){OK=false;Error+=TEXT("link failed; ");}};auto Literal=[&](UEdGraphNode* N,FName Name,const FString& V){if(auto* Pin=P(N,Name))Pin->DefaultValue=V;};
 auto Call=[&](UEdGraph* G,FName Name,UClass* Class=nullptr){auto* F=(Class?Class:UKismetMathLibrary::StaticClass())->FindFunctionByName(Name);check(F);FGraphNodeCreator<UK2Node_CallFunction> C(*G);auto* N=C.CreateNode();N->SetFromFunction(F);C.Finalize();return N;};auto Return=[&](UEdGraphNode* N){return P(N,UEdGraphSchema_K2::PN_ReturnValue);};auto Get=[&](UEdGraph* G,FName Name){FGraphNodeCreator<UK2Node_VariableGet> C(*G);auto* N=C.CreateNode();N->VariableReference.SetSelfMember(Name);C.Finalize();return P(N,Name);};auto Set=[&](FName Name){FGraphNodeCreator<UK2Node_VariableSet> C(*E);auto* N=C.CreateNode();N->VariableReference.SetSelfMember(Name);C.Finalize();return N;};
 auto Vector=[&](double X,double Y,double Z){auto* N=Call(E,TEXT("MakeVector"));Literal(N,TEXT("X"),FString::Printf(TEXT("%.12f"),X));Literal(N,TEXT("Y"),FString::Printf(TEXT("%.12f"),Y));Literal(N,TEXT("Z"),FString::Printf(TEXT("%.12f"),Z));return Return(N);};auto Vec=[&](const TSharedPtr<FJsonObject>& J,const TCHAR* Key){const auto V=J->GetArrayField(Key);return Vector(V[0]->AsNumber(),V[1]->AsNumber(),V[2]->AsNumber());};auto Quat=[&](const TSharedPtr<FJsonObject>& J,const TCHAR* Key){const auto V=J->GetArrayField(Key);auto* N=Call(E,TEXT("MakeQuat"));int32 I=0;for(FName Component:{FName(TEXT("X")),FName(TEXT("Y")),FName(TEXT("Z")),FName(TEXT("W"))})Literal(N,Component,FString::Printf(TEXT("%.12f"),V[I++]->AsNumber()));return Return(N);};
 auto Multiply=[&](UEdGraphPin* A,UEdGraphPin* B){auto* N=Call(E,TEXT("Multiply_QuatQuat"));Link(A,P(N,TEXT("A")));Link(B,P(N,TEXT("B")));return Return(N);};auto Inverse=[&](UEdGraphPin* Q){auto* N=Call(E,TEXT("Quat_Inversed"));Link(Q,P(N,TEXT("Q")));return Return(N);};auto Rotate=[&](UEdGraphPin* Q,UEdGraphPin* V){auto* N=Call(E,TEXT("Quat_RotateVector"));Link(Q,P(N,TEXT("Q")));Link(V,P(N,TEXT("V")));return Return(N);};auto Add=[&](UEdGraphPin* A,UEdGraphPin* B){auto* N=Call(E,TEXT("Add_VectorVector"));Link(A,P(N,TEXT("A")));Link(B,P(N,TEXT("B")));return Return(N);};
 UEdGraphPin *Mesh=nullptr,*Delta=nullptr,*Positive=nullptr,*Negative=nullptr;for(UEdGraphNode* N:E->Nodes){if(auto* V=Cast<UK2Node_VariableGet>(N))if(V->VariableReference.GetMemberName()==TEXT("Mesh"))Mesh=P(V,TEXT("Mesh"));if(auto* V=Cast<UK2Node_VariableSet>(N))if(V->VariableReference.GetMemberName()==TEXT("HcxGaitAlphaR")){if(P(V,TEXT("HcxGaitAlphaR"))->LinkedTo.Num())Positive=P(V,UEdGraphSchema_K2::PN_Then);else Negative=P(V,UEdGraphSchema_K2::PN_Then);}if(auto* V=Cast<UK2Node_Event>(N))if(V->EventReference.GetMemberName()==TEXT("BlueprintUpdateAnimation"))Delta=P(V,TEXT("DeltaTimeX"));}if(!Mesh||!Delta||!Positive||!Negative)return TEXT("width event anchors missing");
 const FName WeightWrite=FootPanel?TEXT("HcxWidthTarget"):TEXT("HcxWidthWeight");auto* Zero=Set(WeightWrite);Literal(Zero,WeightWrite,TEXT("0.0"));Link(Negative,P(Zero,UEdGraphSchema_K2::PN_Execute));auto* Smooth=Call(E,TEXT("FInterpTo"));Link(Get(E,TEXT("HcxWidthWeight")),P(Smooth,TEXT("Current")));Literal(Smooth,TEXT("Target"),TEXT("1.0"));Literal(Smooth,TEXT("InterpSpeed"),TEXT("12.0"));Link(Delta,P(Smooth,TEXT("DeltaTime")));auto* Weight=Set(WeightWrite);Link(Positive,P(Weight,UEdGraphSchema_K2::PN_Execute));if(FootPanel)Literal(Weight,WeightWrite,TEXT("1.0"));else Link(Return(Smooth),P(Weight,WeightWrite));Positive=P(Weight,UEdGraphSchema_K2::PN_Then);
 if(ForceProbe||ParentMovement){
  // Match RetargetPoseFromMesh's attached-parent source; do not assume the skin-part
  // component is owned directly by ACharacter or depends on a locomotion curve.
  auto CastPin=[&](UEdGraphPin* Source,UClass* Class){FGraphNodeCreator<UK2Node_DynamicCast> C(*E);auto* N=C.CreateNode();N->TargetType=Class;N->SetPurity(true);C.Finalize();Link(Source,N->GetCastSourcePin());return N->GetCastResultPin();};
  auto Valid=[&](UEdGraphPin* Object){auto* N=Call(E,TEXT("IsValid"),UKismetSystemLibrary::StaticClass());Link(Object,P(N,TEXT("Object")));return Return(N);};
  auto Select=[&](UEdGraphPin* A,UEdGraphPin* B,UEdGraphPin* Condition){auto* N=Call(E,TEXT("SelectObject"));if(A)Link(A,P(N,TEXT("A")));if(B)Link(B,P(N,TEXT("B")));Link(Condition,P(N,TEXT("bSelectA")));return Return(N);};
  auto* Owning=Return(Call(E,TEXT("GetOwningComponent"),UAnimInstance::StaticClass()));UEdGraphPin* Parent=Owning;UEdGraphPin* Selected=nullptr;
  for(int32 I=0;I<5;++I){auto* N=Call(E,TEXT("GetAttachParent"),USceneComponent::StaticClass());Link(Parent,P(N,UEdGraphSchema_K2::PN_Self));auto* Next=Return(N);auto* Skeletal=CastPin(Next,USkeletalMeshComponent::StaticClass());Selected=Selected?Select(Selected,Skeletal,Valid(Selected)):Skeletal;Parent=CastPin(Select(Next,Owning,Valid(Next)),USceneComponent::StaticClass());}
  Mesh=CastPin(Selected,USkeletalMeshComponent::StaticClass());
  UK2Node_Event* Event=nullptr;for(UEdGraphNode* N:E->Nodes)if(auto* V=Cast<UK2Node_Event>(N))if(V->EventReference.GetMemberName()==TEXT("BlueprintUpdateAnimation"))Event=V;
  auto* ResetL=Set(TEXT("HcxGaitAlphaL"));auto* ResetR=Set(TEXT("HcxGaitAlphaR"));Literal(ResetL,TEXT("HcxGaitAlphaL"),TEXT("0.0"));Literal(ResetR,TEXT("HcxGaitAlphaR"),TEXT("0.0"));Link(P(ResetL,UEdGraphSchema_K2::PN_Then),P(ResetR,UEdGraphSchema_K2::PN_Execute));Link(P(ResetR,UEdGraphSchema_K2::PN_Then),P(Zero,UEdGraphSchema_K2::PN_Execute));
  auto* EventThen=P(Event,UEdGraphSchema_K2::PN_Then);EventThen->BreakAllPinLinks();FGraphNodeCreator<UK2Node_IfThenElse> BC(*E);auto* Branch=BC.CreateNode();BC.Finalize();if(FootPanel){auto* IdleReset=Set(TEXT("HcxIdleTarget"));Literal(IdleReset,TEXT("HcxIdleTarget"),TEXT("0"));Link(EventThen,P(IdleReset,UEdGraphSchema_K2::PN_Execute));Link(P(IdleReset,UEdGraphSchema_K2::PN_Then),P(Branch,UEdGraphSchema_K2::PN_Execute));}else Link(EventThen,P(Branch,UEdGraphSchema_K2::PN_Execute));Link(Valid(Mesh),P(Branch,UEdGraphSchema_K2::PN_Condition));auto* Execute=P(Branch,UEdGraphSchema_K2::PN_Then);Link(P(Branch,UEdGraphSchema_K2::PN_Else),P(ResetL,UEdGraphSchema_K2::PN_Execute));
  if(ParentMovement){
   auto* Owner=Call(E,TEXT("GetOwner"),UActorComponent::StaticClass());Link(Mesh,P(Owner,UEdGraphSchema_K2::PN_Self));UEdGraphPin* MovementOwner=Return(Owner);if(FootPanel){auto* OwnerCharacter=CastPin(MovementOwner,ACharacter::StaticClass());auto* PlayerCharacter=Call(E,TEXT("GetPlayerCharacter"),UGameplayStatics::StaticClass());Link(Owning,P(PlayerCharacter,TEXT("WorldContextObject")));Literal(PlayerCharacter,TEXT("PlayerIndex"),TEXT("0"));MovementOwner=Select(OwnerCharacter,Return(PlayerCharacter),Valid(OwnerCharacter));}FGraphNodeCreator<UK2Node_DynamicCast> CC(*E);auto* Character=CC.CreateNode();Character->TargetType=ACharacter::StaticClass();Character->SetPurity(false);CC.Finalize();Link(MovementOwner,Character->GetCastSourcePin());Link(Execute,P(Character,UEdGraphSchema_K2::PN_Execute));Link(Character->GetInvalidCastPin(),P(ResetL,UEdGraphSchema_K2::PN_Execute));
   FGraphNodeCreator<UK2Node_VariableGet> MC(*E);auto* Movement=MC.CreateNode();Movement->VariableReference.SetExternalMember(TEXT("CharacterMovement"),ACharacter::StaticClass());MC.Finalize();Link(Character->GetCastResultPin(),P(Movement,UEdGraphSchema_K2::PN_Self));auto* Move=P(Movement,TEXT("CharacterMovement"));
   auto* Velocity=Call(E,TEXT("GetVelocity"),AActor::StaticClass());Link(Character->GetCastResultPin(),P(Velocity,UEdGraphSchema_K2::PN_Self));auto* Speed=Call(E,TEXT("VSizeXY"));Link(Return(Velocity),P(Speed,TEXT("A")));auto* Greater=Call(E,TEXT("Greater_DoubleDouble"));Link(Return(Speed),P(Greater,TEXT("A")));Literal(Greater,TEXT("B"),TEXT("20.0"));
   auto* Ground=Call(E,TEXT("IsMovingOnGround"),UNavMovementComponent::StaticClass());Link(Move,P(Ground,UEdGraphSchema_K2::PN_Self));auto* Crouch=Call(E,TEXT("IsCrouching"),UNavMovementComponent::StaticClass());Link(Move,P(Crouch,UEdGraphSchema_K2::PN_Self));auto* Anim=Call(E,TEXT("GetAnimInstance"),USkeletalMeshComponent::StaticClass());Link(Mesh,P(Anim,UEdGraphSchema_K2::PN_Self));auto* Montage=Call(E,TEXT("IsAnyMontagePlaying"),UAnimInstance::StaticClass());Link(Return(Anim),P(Montage,UEdGraphSchema_K2::PN_Self));
   auto Not=[&](UEdGraphPin* V){auto* N=Call(E,TEXT("Not_PreBool"));Link(V,P(N,TEXT("A")));return Return(N);};auto And=[&](UEdGraphPin* A,UEdGraphPin* B){auto* N=Call(E,TEXT("BooleanAND"));Link(A,P(N,TEXT("A")));Link(B,P(N,TEXT("B")));return Return(N);};
   auto* GateExec=Character->GetValidCastPin();
   if(FootPanel){auto BoolFloat=[&](UEdGraphPin* Value){auto* N=Call(E,TEXT("SelectFloat"));Literal(N,TEXT("A"),TEXT("1"));Literal(N,TEXT("B"),TEXT("0"));Link(Value,P(N,TEXT("bPickA")));return Return(N);};for(auto Pair:{TPair<FName,UEdGraphPin*>(TEXT("HcxFootDebugSpeed"),Return(Speed)),TPair<FName,UEdGraphPin*>(TEXT("HcxFootDebugGround"),BoolFloat(Return(Ground))),TPair<FName,UEdGraphPin*>(TEXT("HcxFootDebugMontage"),BoolFloat(Return(Montage)))}){auto* Store=Set(Pair.Key);Link(GateExec,P(Store,UEdGraphSchema_K2::PN_Execute));Link(Pair.Value,P(Store,Pair.Key));GateExec=P(Store,UEdGraphSchema_K2::PN_Then);}}
   if(FootPanel){auto* Idle=Call(E,TEXT("SelectFloat"));Link(Get(E,TEXT("HcxIdleEnabled")),P(Idle,TEXT("A")));Literal(Idle,TEXT("B"),TEXT("0"));Link(And(And(Not(Return(Greater)),Return(Ground)),And(Not(Return(Crouch)),Not(Return(Montage)))),P(Idle,TEXT("bPickA")));auto* IdleStore=Set(TEXT("HcxIdleTarget"));Link(GateExec,P(IdleStore,UEdGraphSchema_K2::PN_Execute));Link(Return(Idle),P(IdleStore,TEXT("HcxIdleTarget")));GateExec=P(IdleStore,UEdGraphSchema_K2::PN_Then);}
   FGraphNodeCreator<UK2Node_IfThenElse> GC(*E);auto* Gate=GC.CreateNode();GC.Finalize();Link(GateExec,P(Gate,UEdGraphSchema_K2::PN_Execute));Link(And(And(Return(Greater),Return(Ground)),And(Not(Return(Crouch)),Not(Return(Montage)))),P(Gate,UEdGraphSchema_K2::PN_Condition));Link(P(Gate,UEdGraphSchema_K2::PN_Else),P(ResetL,UEdGraphSchema_K2::PN_Execute));Execute=P(Gate,UEdGraphSchema_K2::PN_Then);
  }
  for(FName Name:{FName(TEXT("HcxGaitAlphaL")),FName(TEXT("HcxGaitAlphaR"))}){auto* N=Set(Name);Literal(N,Name,TEXT("1.0"));Link(Execute,P(N,UEdGraphSchema_K2::PN_Execute));Execute=P(N,UEdGraphSchema_K2::PN_Then);}
  P(Weight,UEdGraphSchema_K2::PN_Execute)->BreakAllPinLinks();if(ForceProbe){P(Weight,WeightWrite)->BreakAllPinLinks();Literal(Weight,WeightWrite,TEXT("1.0"));}Link(Execute,P(Weight,UEdGraphSchema_K2::PN_Execute));
 }
 auto Socket=[&](const FString& Bone){auto* N=Call(E,TEXT("GetSocketRotation"),USceneComponent::StaticClass());Link(Mesh,P(N,UEdGraphSchema_K2::PN_Self));Literal(N,TEXT("InSocketName"),Bone);auto* Q=Call(E,TEXT("Conv_RotatorToQuaternion"));Link(Return(N),P(Q,TEXT("InRot")));return Return(Q);};auto* ComponentRotation=Call(E,TEXT("K2_GetComponentRotation"),USceneComponent::StaticClass());Link(Mesh,P(ComponentRotation,UEdGraphSchema_K2::PN_Self));
 auto PanelValue=[&](FName Name){return Get(E,Name==TEXT("LeftOutward")?TEXT("HcxEffectiveAngleL"):TEXT("HcxEffectiveAngleR"));};
 for(bool Left:{true,false}){
  auto J=Settings->GetObjectField(Left?TEXT("L"):TEXT("R"));const FString S=Left?TEXT("l"):TEXT("r");auto* Offset=Call(E,TEXT("GreaterGreater_VectorRotator"));auto* WidthVector=Vector((Left?1:-1)*Settings->GetNumberField(TEXT("offset_cm_per_foot")),0,0);if(FootPanel){auto* Sign=Call(E,TEXT("Multiply_DoubleDouble"));Link(Get(E,TEXT("HcxEffectiveWidth")),P(Sign,TEXT("A")));Literal(Sign,TEXT("B"),Left?TEXT("1"):TEXT("-1"));Link(Return(Sign),P(WidthVector->GetOwningNode(),TEXT("X")));}Link(WidthVector,P(Offset,TEXT("A")));Link(Return(ComponentRotation),P(Offset,TEXT("B")));auto* WorldOffset=Return(Offset);
  auto* Thigh=Multiply(Socket(TEXT("thigh_")+S),Quat(J,TEXT("thigh_offset_quat")));auto* Calf=Multiply(Socket(TEXT("calf_")+S),Quat(J,TEXT("calf_offset_quat")));auto* Foot=Multiply(Socket(TEXT("foot_")+S),Quat(J,TEXT("foot_offset_quat")));auto* Pelvis=Multiply(Socket(TEXT("pelvis")),Quat(J,TEXT("pelvis_offset_quat")));auto* KneeVector=Rotate(Thigh,Vec(J,TEXT("calf_local_translation")));auto* EndVector=Add(KneeVector,Rotate(Calf,Vec(J,TEXT("foot_local_translation"))));
  auto* PoleWorld=Call(E,TEXT("GreaterGreater_VectorRotator"));auto* KneePoleVector=Vector((Left?-1:1)*KneeInward,0,0);if(FootPanel){auto* Sign=Call(E,TEXT("Multiply_DoubleDouble"));Link(Get(E,TEXT("HcxEffectiveKnee")),P(Sign,TEXT("A")));Literal(Sign,TEXT("B"),Left?TEXT("-1"):TEXT("1"));Link(Return(Sign),P(KneePoleVector->GetOwningNode(),TEXT("X")));}Link(KneePoleVector,P(PoleWorld,TEXT("A")));Link(Return(ComponentRotation),P(PoleWorld,TEXT("B")));auto* PoleOffset=Return(PoleWorld);
  auto* IK=Call(E,TEXT("K2_TwoBoneIK"),UKismetAnimationLibrary::StaticClass());Link(Vector(0,0,0),P(IK,TEXT("RootPos")));Link(KneeVector,P(IK,TEXT("JointPos")));Link(EndVector,P(IK,TEXT("EndPos")));Link(Add(KneeVector,PoleOffset),P(IK,TEXT("JointTarget")));Link(Add(EndVector,WorldOffset),P(IK,TEXT("Effector")));Literal(IK,TEXT("bAllowStretching"),TEXT("false"));
  auto* Between=Call(E,TEXT("Quat_FindBetweenVectors"));Link(KneeVector,P(Between,TEXT("Start")));Link(P(IK,TEXT("OutJointPos")),P(Between,TEXT("End")));auto* NewThigh=Multiply(Return(Between),Thigh);auto* RollLocal=Multiply(Multiply(Inverse(Pelvis),NewThigh),Quat(J,TEXT("roll_offset_quat")));auto* RollRot=Call(E,TEXT("Quat_Rotator"));Link(RollLocal,P(RollRot,TEXT("Q")));auto* LocalOffset=Rotate(Inverse(Foot),WorldOffset);
  if(BackFollow>0||FootUntwist>0||FootTwistLock){
   auto Sub=[&](UEdGraphPin* A,UEdGraphPin* B){auto* N=Call(E,TEXT("Subtract_VectorVector"));Link(A,P(N,TEXT("A")));Link(B,P(N,TEXT("B")));return Return(N);};
   auto Dot=[&](UEdGraphPin* A,UEdGraphPin* B){auto* N=Call(E,TEXT("Dot_VectorVector"));Link(A,P(N,TEXT("A")));Link(B,P(N,TEXT("B")));return Return(N);};
   auto* CalfDelta=Call(E,TEXT("Quat_FindBetweenVectors"));Link(Sub(EndVector,KneeVector),P(CalfDelta,TEXT("Start")));Link(Sub(P(IK,TEXT("OutEndPos")),P(IK,TEXT("OutJointPos"))),P(CalfDelta,TEXT("End")));
   UEdGraphPin* CorrectionQ=nullptr;
   if(FootUntwist>0||FootTwistLock){
    // Extract signed axial twist of the foot relative to the solved calf and
    // the target foot's own bind rotation. Only rotate the foot about the calf
    // axis; never change the ankle location or the hip/knee solution.
    const FReferenceSkeleton& Ref=TwistReference->GetRefSkeleton();const auto& Pose=Ref.GetRefBonePose();
    const int32 FootIndex=Ref.FindBoneIndex(FName(*(TEXT("SKEL_")+FString(Left?TEXT("L"):TEXT("R"))+TEXT("_Foot"))));
    if(FootIndex==INDEX_NONE)return TEXT("foot twist reference bone missing");
    const int32 CalfIndex=Ref.GetParentIndex(FootIndex);FQuat RefCalf=FQuat::Identity;
    for(int32 I=CalfIndex;I!=INDEX_NONE;I=Ref.GetParentIndex(I))RefCalf=Pose[I].GetRotation()*RefCalf;
    const FVector Axis=Pose[FootIndex].GetTranslation().GetSafeNormal();
    const double InwardSign=FVector::DotProduct(FVector::CrossProduct(RefCalf.RotateVector(Axis),FVector(0,1,0)),FVector(Left?-1:1,0,0))>=0?1:-1;
    auto ConstQ=[&](FQuat Q){auto* N=Call(E,TEXT("MakeQuat"));Literal(N,TEXT("X"),FString::SanitizeFloat(Q.X));Literal(N,TEXT("Y"),FString::SanitizeFloat(Q.Y));Literal(N,TEXT("Z"),FString::SanitizeFloat(Q.Z));Literal(N,TEXT("W"),FString::SanitizeFloat(Q.W));return Return(N);};
    auto Scalar=[&](FName Function,UEdGraphPin* A,double B){auto* N=Call(E,Function);Link(A,P(N,TEXT("A")));Literal(N,TEXT("B"),FString::SanitizeFloat(B));return Return(N);};
    auto* SolvedCalf=Multiply(Return(CalfDelta),Calf);
    auto* BodyQ=Call(E,TEXT("Conv_RotatorToQuaternion"));Link(Return(ComponentRotation),P(BodyQ,TEXT("InRot")));
    const FQuat Yaw(FVector::UpVector,FMath::DegreesToRadians(Left?-ToeOut:ToeOut));
    auto* YawedFoot=Multiply(Multiply(Multiply(Return(BodyQ),ConstQ(Yaw)),Inverse(Return(BodyQ))),Foot);
    auto* Relative=Multiply(Multiply(Inverse(SolvedCalf),YawedFoot),ConstQ(Pose[FootIndex].GetRotation().Inverse()));
    auto* Parts=Call(E,TEXT("BreakQuat"));Link(Relative,P(Parts,TEXT("InQuat")));auto* XYZ=Call(E,TEXT("MakeVector"));for(FName Name:{FName(TEXT("X")),FName(TEXT("Y")),FName(TEXT("Z"))})Link(P(Parts,Name),P(XYZ,Name));
    auto* Projection=Dot(Return(XYZ),Vector(Axis.X,Axis.Y,Axis.Z));auto* Atan=Call(E,TEXT("DegAtan2"));Link(Projection,P(Atan,TEXT("Y")));Link(P(Parts,TEXT("W")),P(Atan,TEXT("X")));
    auto* Signed=Call(E,TEXT("NormalizeAxis"));Link(Scalar(TEXT("Multiply_DoubleDouble"),Return(Atan),2.0),P(Signed,TEXT("Angle")));
    if(FootTwistLock){
     // Remove the entire left twist factor about the calf axis. Write the
     // remaining swing * target bind rotation directly in parent-bone space,
     // so the final foot follows the actual IK calf instead of a world yaw.
     auto* UntwistAngle=Scalar(TEXT("Multiply_DoubleDouble"),Return(Signed),-1.0);
     if(FootPanel){const FName Used=Left?TEXT("HcxFootUsedL"):TEXT("HcxFootUsedR");auto* Store=Set(Used);Link(Positive,P(Store,UEdGraphSchema_K2::PN_Execute));Link(PanelValue(Left?TEXT("LeftOutward"):TEXT("RightOutward")),P(Store,Used));Positive=P(Store,UEdGraphSchema_K2::PN_Then);auto* Sum=Call(E,TEXT("Add_DoubleDouble"));Link(UntwistAngle,P(Sum,TEXT("A")));Link(Scalar(TEXT("Multiply_DoubleDouble"),Get(E,Used),-InwardSign),P(Sum,TEXT("B")));UntwistAngle=Return(Sum);}
     auto* Untwist=Call(E,TEXT("RotatorFromAxisAndAngle"));Link(Vector(Axis.X,Axis.Y,Axis.Z),P(Untwist,TEXT("Axis")));Link(UntwistAngle,P(Untwist,TEXT("Angle")));
     auto* UntwistQ=Call(E,TEXT("Conv_RotatorToQuaternion"));Link(Return(Untwist),P(UntwistQ,TEXT("InRot")));
     CorrectionQ=Multiply(Multiply(Return(UntwistQ),Relative),ConstQ(Pose[FootIndex].GetRotation()));
    }else{
    auto* Excess=Scalar(TEXT("Subtract_DoubleDouble"),Scalar(TEXT("Multiply_DoubleDouble"),Return(Signed),InwardSign),5.0);
    auto* Cap=Call(E,TEXT("FClamp"));Link(Excess,P(Cap,TEXT("Value")));Literal(Cap,TEXT("Min"),TEXT("0.0"));Literal(Cap,TEXT("Max"),FString::SanitizeFloat(FootUntwist));
    auto* Fix=Call(E,TEXT("RotatorFromAxisAndAngle"));Link(Rotate(SolvedCalf,Vector(Axis.X,Axis.Y,Axis.Z)),P(Fix,TEXT("Axis")));Link(Scalar(TEXT("Multiply_DoubleDouble"),Return(Cap),-InwardSign),P(Fix,TEXT("Angle")));
    auto* FixQ=Call(E,TEXT("Conv_RotatorToQuaternion"));Link(Return(Fix),P(FixQ,TEXT("InRot")));CorrectionQ=Return(FixQ);
    }
   }else{
   auto* Angle=Call(E,TEXT("Quat_GetAngle"));Link(Return(CalfDelta),P(Angle,TEXT("Q")));auto* Safe=Call(E,TEXT("FMax"));Link(Return(Angle),P(Safe,TEXT("A")));Literal(Safe,TEXT("B"),TEXT("0.0001"));auto* Ratio=Call(E,TEXT("Divide_DoubleDouble"));Literal(Ratio,TEXT("A"),FString::SanitizeFloat(FMath::DegreesToRadians(BackFollow)));Link(Return(Safe),P(Ratio,TEXT("B")));auto* Limit=Call(E,TEXT("FClamp"));Link(Return(Ratio),P(Limit,TEXT("Value")));Literal(Limit,TEXT("Min"),TEXT("0.0"));Literal(Limit,TEXT("Max"),TEXT("1.0"));
   auto* Identity=Call(E,TEXT("MakeQuat"));Literal(Identity,TEXT("X"),TEXT("0"));Literal(Identity,TEXT("Y"),TEXT("0"));Literal(Identity,TEXT("Z"),TEXT("0"));Literal(Identity,TEXT("W"),TEXT("1"));auto* Bound=Call(E,TEXT("Quat_Slerp"));Link(Return(Identity),P(Bound,TEXT("A")));Link(Return(CalfDelta),P(Bound,TEXT("B")));Link(Return(Limit),P(Bound,TEXT("Alpha")));
   CorrectionQ=Return(Bound);
   }
   auto* ComponentQ=Call(E,TEXT("Conv_RotatorToQuaternion"));Link(Return(ComponentRotation),P(ComponentQ,TEXT("InRot")));auto* LocalDelta=FootTwistLock?CorrectionQ:Multiply(Multiply(Inverse(Return(ComponentQ)),CorrectionQ),Return(ComponentQ));auto* Rotation=Call(E,TEXT("Quat_Rotator"));Link(LocalDelta,P(Rotation,TEXT("Q")));
   auto Location=[&](const FString& Bone){auto* N=Call(E,TEXT("GetSocketLocation"),USceneComponent::StaticClass());Link(Mesh,P(N,UEdGraphSchema_K2::PN_Self));Literal(N,TEXT("InSocketName"),Bone);return Return(N);};
   auto* Forward=Rotate(Return(ComponentQ),Vector(0,1,0));auto* Up=Rotate(Return(ComponentQ),Vector(0,0,1));auto* Y=Dot(EndVector,Forward);auto* Z=Dot(Sub(Location(TEXT("ball_")+S),Location(TEXT("root"))),Up);
   auto* Back=Call(E,TEXT("Subtract_DoubleDouble"));Literal(Back,TEXT("A"),TEXT("-5.0"));Link(Y,P(Back,TEXT("B")));auto* High=Call(E,TEXT("Subtract_DoubleDouble"));Link(Z,P(High,TEXT("A")));Literal(High,TEXT("B"),TEXT("12.0"));
   auto Fade=[&](UEdGraphPin* Value){auto* Divide=Call(E,TEXT("Divide_DoubleDouble"));Link(Value,P(Divide,TEXT("A")));Literal(Divide,TEXT("B"),TEXT("15.0"));auto* C=Call(E,TEXT("FClamp"));Link(Return(Divide),P(C,TEXT("Value")));Literal(C,TEXT("Min"),TEXT("0.0"));Literal(C,TEXT("Max"),TEXT("1.0"));return Return(C);};auto* Product=Call(E,TEXT("Multiply_DoubleDouble"));Link(Fade(Return(Back)),P(Product,TEXT("A")));Link(Fade(Return(High)),P(Product,TEXT("B")));
   for(auto Pair:{TPair<FName,UEdGraphPin*>(Left?TEXT("HcxFootFollowL"):TEXT("HcxFootFollowR"),Return(Rotation)),TPair<FName,UEdGraphPin*>(Left?TEXT("HcxFootBackL"):TEXT("HcxFootBackR"),Return(Product))}){auto* Store=Set(Pair.Key);Link(Positive,P(Store,UEdGraphSchema_K2::PN_Execute));Link(Pair.Value,P(Store,Pair.Key));Positive=P(Store,UEdGraphSchema_K2::PN_Then);}
  }
  const FName OffsetName=Left?TEXT("HcxWidthOffsetL"):TEXT("HcxWidthOffsetR"),RollName=Left?TEXT("HcxWidthRollL"):TEXT("HcxWidthRollR"),PoleName=Left?TEXT("HcxWidthPoleL"):TEXT("HcxWidthPoleR");for(auto Pair:{TPair<FName,UEdGraphPin*>(OffsetName,LocalOffset),TPair<FName,UEdGraphPin*>(RollName,Return(RollRot)),TPair<FName,UEdGraphPin*>(PoleName,Rotate(Inverse(Calf),PoleOffset))}){auto* Store=Set(Pair.Key);Link(Positive,P(Store,UEdGraphSchema_K2::PN_Execute));Link(Pair.Value,P(Store,Pair.Key));Positive=P(Store,UEdGraphSchema_K2::PN_Then);}
 }
 if(FootPanel){
  // Both state outcomes converge here. Keep solving the current pose while
  // fading out, rather than retaining the last moving-frame IK inputs.
  auto CalcLinks=P(Weight,UEdGraphSchema_K2::PN_Then)->LinkedTo;P(Weight,UEdGraphSchema_K2::PN_Then)->BreakAllPinLinks();
  auto* Duration=Call(E,TEXT("FMax"));Link(Get(E,TEXT("HcxBlendSeconds")),P(Duration,TEXT("A")));Literal(Duration,TEXT("B"),TEXT("0.05"));auto* Rate=Call(E,TEXT("Divide_DoubleDouble"));Literal(Rate,TEXT("A"),TEXT("1"));Link(Return(Duration),P(Rate,TEXT("B")));
  auto* Interp=Call(E,TEXT("FInterpTo_Constant"));Link(Get(E,TEXT("HcxWidthWeight")),P(Interp,TEXT("Current")));Link(Get(E,TEXT("HcxWidthTarget")),P(Interp,TEXT("Target")));Link(Delta,P(Interp,TEXT("DeltaTime")));Link(Return(Rate),P(Interp,TEXT("InterpSpeed")));
  auto* Store=Set(TEXT("HcxWidthWeight"));Link(Return(Interp),P(Store,TEXT("HcxWidthWeight")));Link(P(Weight,UEdGraphSchema_K2::PN_Then),P(Store,UEdGraphSchema_K2::PN_Execute));Link(P(Zero,UEdGraphSchema_K2::PN_Then),P(Store,UEdGraphSchema_K2::PN_Execute));
  auto* Valid=Call(E,TEXT("IsValid"),UKismetSystemLibrary::StaticClass());Link(Mesh,P(Valid,TEXT("Object")));FGraphNodeCreator<UK2Node_IfThenElse> BC(*E);auto* Branch=BC.CreateNode();BC.Finalize();UEdGraphPin* BlendExec=P(Store,UEdGraphSchema_K2::PN_Then);
  auto Save=[&](FName Name,UEdGraphPin* Value){auto* N=Set(Name);Link(BlendExec,P(N,UEdGraphSchema_K2::PN_Execute));Link(Value,P(N,Name));BlendExec=P(N,UEdGraphSchema_K2::PN_Then);};
  auto FloatOp=[&](FName Function,UEdGraphPin* A,UEdGraphPin* B){auto* N=Call(E,Function);Link(A,P(N,TEXT("A")));Link(B,P(N,TEXT("B")));return Return(N);};
  auto Number=[&](double Value){auto* N=Call(E,TEXT("MakeLiteralDouble"),UKismetSystemLibrary::StaticClass());Literal(N,TEXT("Value"),FString::SanitizeFloat(Value));return Return(N);};
  auto Clamp01=[&](UEdGraphPin* Value){auto* N=Call(E,TEXT("FClamp"));Link(Value,P(N,TEXT("Value")));Literal(N,TEXT("Min"),TEXT("0"));Literal(N,TEXT("Max"),TEXT("1"));return Return(N);};
  auto* IdleInterp=Call(E,TEXT("FInterpTo_Constant"));Link(Get(E,TEXT("HcxIdleWeight")),P(IdleInterp,TEXT("Current")));Link(Get(E,TEXT("HcxIdleTarget")),P(IdleInterp,TEXT("Target")));Link(Delta,P(IdleInterp,TEXT("DeltaTime")));Link(Return(Rate),P(IdleInterp,TEXT("InterpSpeed")));Save(TEXT("HcxIdleWeight"),Return(IdleInterp));
  Save(TEXT("HcxLegWeight"),Clamp01(FloatOp(TEXT("Add_DoubleDouble"),Get(E,TEXT("HcxWidthWeight")),Get(E,TEXT("HcxIdleWeight")))));
  auto Average=[&](UEdGraphPin* A,UEdGraphPin* AW,UEdGraphPin* B,UEdGraphPin* BW,UEdGraphPin* Total){return FloatOp(TEXT("Divide_DoubleDouble"),FloatOp(TEXT("Add_DoubleDouble"),FloatOp(TEXT("Multiply_DoubleDouble"),A,AW),FloatOp(TEXT("Multiply_DoubleDouble"),B,BW)),FloatOp(TEXT("FMax"),Total,Number(.0001)));};
  Save(TEXT("HcxEffectiveWidth"),Average(Get(E,TEXT("HcxWidthCm")),Get(E,TEXT("HcxWidthWeight")),Get(E,TEXT("HcxIdleWidthCm")),Get(E,TEXT("HcxIdleWeight")),Get(E,TEXT("HcxLegWeight"))));
  Save(TEXT("HcxEffectiveKnee"),Average(Get(E,TEXT("HcxKneeCm")),Get(E,TEXT("HcxWidthWeight")),Get(E,TEXT("HcxIdleKneeCm")),Get(E,TEXT("HcxIdleWeight")),Get(E,TEXT("HcxLegWeight"))));
  auto* MoveFoot=FloatOp(TEXT("Multiply_DoubleDouble"),Get(E,TEXT("HcxWidthWeight")),Get(E,TEXT("HcxFootEnabled")));auto* IdleFoot=FloatOp(TEXT("Multiply_DoubleDouble"),FloatOp(TEXT("Multiply_DoubleDouble"),Get(E,TEXT("HcxIdleWeight")),Get(E,TEXT("HcxIdleFootEnabled"))),Get(E,TEXT("HcxFootEnabled")));auto* FootTotal=Clamp01(FloatOp(TEXT("Add_DoubleDouble"),MoveFoot,IdleFoot));
  Save(TEXT("HcxFootNormalWeight"),FloatOp(TEXT("Multiply_DoubleDouble"),FootTotal,FloatOp(TEXT("Subtract_DoubleDouble"),Number(1),Get(E,TEXT("HcxFootPreview")))));
  for(bool Left:{true,false})Save(Left?TEXT("HcxEffectiveAngleL"):TEXT("HcxEffectiveAngleR"),Average(Get(E,Left?TEXT("HcxFootRequestedL"):TEXT("HcxFootRequestedR")),MoveFoot,Get(E,TEXT("HcxIdleAngle")),IdleFoot,FootTotal));
  Link(BlendExec,P(Branch,UEdGraphSchema_K2::PN_Execute));Link(Return(Valid),P(Branch,UEdGraphSchema_K2::PN_Condition));for(auto* Pin:CalcLinks)Link(P(Branch,UEdGraphSchema_K2::PN_Then),Pin);
 }
 UEdGraph* G=nullptr;UAnimGraphNode_Root* Root=nullptr;TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);for(auto* Graph:Graphs)for(UEdGraphNode* N:Graph->Nodes)if(auto* V=Cast<UAnimGraphNode_Root>(N)){G=Graph;Root=V;}if(!Root||P(Root,TEXT("Result"))->LinkedTo.Num()!=1)return TEXT("width root missing");auto* Current=P(Root,TEXT("Result"))->LinkedTo[0];P(Root,TEXT("Result"))->BreakAllPinLinks();auto Out=[](UEdGraphNode* N)->UEdGraphPin*{for(auto* Pin:N->Pins)if(Pin->Direction==EGPD_Output)return Pin;return nullptr;};
 if(FootPanel)for(auto* Node:TArray<UEdGraphNode*>(G->Nodes))if(auto* V=Cast<UK2Node_VariableGet>(Node)){const FName Name=V->VariableReference.GetMemberName();if(Name==TEXT("HcxGaitAlphaL")||Name==TEXT("HcxGaitAlphaR")){auto* Old=P(V,Name);auto Links=Old->LinkedTo;Old->BreakAllPinLinks();auto* Shared=Get(G,TEXT("HcxWidthWeight"));for(auto* Pin:Links)Link(Shared,Pin);}}
 FGraphNodeCreator<UAnimGraphNode_LocalToComponentSpace> LC(*G);auto* Local=LC.CreateNode();LC.Finalize();Link(Current,P(Local,TEXT("LocalPose")));Current=Out(Local);
 for(bool Left:{true,false}){
  const FName Foot=Left?TEXT("SKEL_L_Foot"):TEXT("SKEL_R_Foot"),Calf=Left?TEXT("SKEL_L_Calf"):TEXT("SKEL_R_Calf");FGraphNodeCreator<UAnimGraphNode_TwoBoneIK> C(*G);auto* N=C.CreateNode();N->Node.IKBone.BoneName=Foot;N->Node.EffectorTarget=FBoneSocketTarget(Foot);N->Node.JointTarget=FBoneSocketTarget(Calf);N->Node.EffectorLocationSpace=BCS_BoneSpace;N->Node.JointTargetLocationSpace=BCS_BoneSpace;N->Node.JointTargetLocation=FVector::ZeroVector;N->Node.bAllowStretching=false;N->Node.bTakeRotationFromEffectorSpace=false;N->Node.bMaintainEffectorRelRot=false;N->Node.bAllowTwist=true;N->Node.Alpha=1;C.Finalize();
  if(!N->FindPin(TEXT("Alpha")))for(int32 I=0;I<N->ShowPinForProperties.Num();++I)if(N->ShowPinForProperties[I].PropertyName==TEXT("Alpha")){N->SetPinVisibility(true,I);break;}Link(Get(G,FootPanel?TEXT("HcxLegWeight"):TEXT("HcxWidthWeight")),P(N,TEXT("Alpha")));Link(Get(G,Left?TEXT("HcxWidthOffsetL"):TEXT("HcxWidthOffsetR")),P(N,TEXT("EffectorLocation")));Link(Get(G,Left?TEXT("HcxWidthPoleL"):TEXT("HcxWidthPoleR")),P(N,TEXT("JointTargetLocation")));Link(Current,P(N,TEXT("ComponentPose")));Current=Out(N);
  FGraphNodeCreator<UAnimGraphNode_ModifyBone> RC(*G);auto* Roll=RC.CreateNode();Roll->Node.BoneToModify.BoneName=Left?TEXT("RB_L_ThighRoll"):TEXT("RB_R_ThighRoll");Roll->Node.RotationMode=BMM_Replace;Roll->Node.RotationSpace=BCS_ParentBoneSpace;Roll->Node.Alpha=1;RC.Finalize();for(FName Name:{FName(TEXT("Rotation")),FName(TEXT("Alpha"))})if(!Roll->FindPin(Name))for(int32 I=0;I<Roll->ShowPinForProperties.Num();++I)if(Roll->ShowPinForProperties[I].PropertyName==Name){Roll->SetPinVisibility(true,I);break;}Link(Get(G,FootPanel?TEXT("HcxLegWeight"):TEXT("HcxWidthWeight")),P(Roll,TEXT("Alpha")));Link(Get(G,Left?TEXT("HcxWidthRollL"):TEXT("HcxWidthRollR")),P(Roll,TEXT("Rotation")));Link(Current,P(Roll,TEXT("ComponentPose")));Current=Out(Roll);
 }
 // Apply only a final foot yaw; the hip/knee solution and ankle target stay intact.
 if(FMath::Abs(ToeOut)>KINDA_SMALL_NUMBER)for(bool Left:{true,false}){
  FGraphNodeCreator<UAnimGraphNode_ModifyBone> FC(*G);auto* FootYaw=FC.CreateNode();FootYaw->Node.BoneToModify.BoneName=Left?TEXT("SKEL_L_Foot"):TEXT("SKEL_R_Foot");FootYaw->Node.RotationMode=BMM_Additive;FootYaw->Node.RotationSpace=BCS_ComponentSpace;FootYaw->Node.Rotation=FRotator(0,Left?-ToeOut:ToeOut,0);FootYaw->Node.Alpha=1;FC.Finalize();
  if(!FootYaw->FindPin(TEXT("Alpha")))for(int32 I=0;I<FootYaw->ShowPinForProperties.Num();++I)if(FootYaw->ShowPinForProperties[I].PropertyName==TEXT("Alpha")){FootYaw->SetPinVisibility(true,I);break;}
  if(FootYawProbe)Literal(FootYaw,TEXT("Alpha"),TEXT("1.0"));else Link(Get(G,TEXT("HcxWidthWeight")),P(FootYaw,TEXT("Alpha")));Link(Current,P(FootYaw,TEXT("ComponentPose")));Current=Out(FootYaw);
 }
 if(BackFollow>0||FootUntwist>0||FootTwistLock)for(bool Left:{true,false}){
  FGraphNodeCreator<UAnimGraphNode_ModifyBone> FC(*G);auto* N=FC.CreateNode();N->Node.BoneToModify.BoneName=Left?TEXT("SKEL_L_Foot"):TEXT("SKEL_R_Foot");N->Node.RotationMode=FootTwistLock?BMM_Replace:BMM_Additive;N->Node.RotationSpace=FootTwistLock?BCS_ParentBoneSpace:BCS_ComponentSpace;N->Node.Alpha=1;FC.Finalize();for(FName Name:{FName(TEXT("Rotation")),FName(TEXT("Alpha"))})if(!N->FindPin(Name))for(int32 I=0;I<N->ShowPinForProperties.Num();++I)if(N->ShowPinForProperties[I].PropertyName==Name){N->SetPinVisibility(true,I);break;}
  if(FootTwistLock){Link(Get(G,FootPanel?TEXT("HcxFootNormalWeight"):TEXT("HcxWidthWeight")),P(N,TEXT("Alpha")));}else{auto* Product=Call(G,TEXT("Multiply_DoubleDouble"));Link(Get(G,TEXT("HcxWidthWeight")),P(Product,TEXT("A")));Link(Get(G,Left?TEXT("HcxFootBackL"):TEXT("HcxFootBackR")),P(Product,TEXT("B")));Link(Return(Product),P(N,TEXT("Alpha")));}Link(Get(G,Left?TEXT("HcxFootFollowL"):TEXT("HcxFootFollowR")),P(N,TEXT("Rotation")));Link(Current,P(N,TEXT("ComponentPose")));Current=Out(N);
 }
 if(FootPanel)for(bool Left:{true,false}){
  FGraphNodeCreator<UAnimGraphNode_ModifyBone> PC(*G);auto* N=PC.CreateNode();N->Node.BoneToModify.BoneName=Left?TEXT("SKEL_L_Foot"):TEXT("SKEL_R_Foot");N->Node.RotationMode=BMM_Additive;N->Node.RotationSpace=BCS_ComponentSpace;N->Node.Alpha=1;PC.Finalize();for(FName Name:{FName(TEXT("Rotation")),FName(TEXT("Alpha"))})if(!N->FindPin(Name))for(int32 I=0;I<N->ShowPinForProperties.Num();++I)if(N->ShowPinForProperties[I].PropertyName==Name){N->SetPinVisibility(true,I);break;}
  auto* Angle=Call(G,TEXT("Multiply_DoubleDouble"));Link(Get(G,Left?TEXT("HcxFootRequestedL"):TEXT("HcxFootRequestedR")),P(Angle,TEXT("A")));Literal(Angle,TEXT("B"),Left?TEXT("-1"):TEXT("1"));auto* Rot=Call(G,TEXT("MakeRotator"));Link(Return(Angle),P(Rot,TEXT("Yaw")));Link(Return(Rot),P(N,TEXT("Rotation")));auto* Alpha=Call(G,TEXT("Multiply_DoubleDouble"));Link(Get(G,TEXT("HcxFootPreview")),P(Alpha,TEXT("A")));Link(Get(G,TEXT("HcxFootEnabled")),P(Alpha,TEXT("B")));Link(Return(Alpha),P(N,TEXT("Alpha")));Link(Current,P(N,TEXT("ComponentPose")));Current=Out(N);
 }
 FGraphNodeCreator<UAnimGraphNode_ComponentToLocalSpace> CL(*G);auto* Component=CL.CreateNode();CL.Finalize();Link(Current,P(Component,TEXT("ComponentPose")));Link(Out(Component),P(Root,TEXT("Result")));
 if(!OK)return Error;FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);BP->MarkPackageDirty();return BP->Status==BS_Error?TEXT("width blueprint compilation failed"):TEXT("OK");
}
