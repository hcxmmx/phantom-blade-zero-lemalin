#include "HcxRetargetTools.h"
#include "Animation/AnimBlueprint.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_ModifyBone.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

FString UHcxRetargetTools::BuildHipGraph(UAnimBlueprint* BP,const FString& SettingsJson){
 TSharedPtr<FJsonObject> Settings;if(!BP||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(SettingsJson),Settings))return TEXT("hip settings missing");
 FEdGraphPinType FloatType;FloatType.PinCategory=UEdGraphSchema_K2::PC_Real;FloatType.PinSubCategory=UEdGraphSchema_K2::PC_Float;
 FEdGraphPinType RotatorType;RotatorType.PinCategory=UEdGraphSchema_K2::PC_Struct;RotatorType.PinSubCategoryObject=TBaseStructure<FRotator>::Get();
 for(FName Name:{FName(TEXT("HcxHipWeight")),FName(TEXT("HcxHipAngleL")),FName(TEXT("HcxHipAngleR"))})FBlueprintEditorUtils::AddMemberVariable(BP,Name,FloatType,TEXT("0.0"));
 for(FName Name:{FName(TEXT("HcxHipTargetL")),FName(TEXT("HcxHipTargetR")),FName(TEXT("HcxRollTargetL")),FName(TEXT("HcxRollTargetR"))})FBlueprintEditorUtils::AddMemberVariable(BP,Name,RotatorType);
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);bool OK=true;FString Error;UEdGraph* E=BP->UbergraphPages[0];
 auto P=[&](UEdGraphNode* N,FName Name)->UEdGraphPin*{auto* Pin=N?N->FindPin(Name):nullptr;if(!Pin){OK=false;Error+=TEXT("missing pin ")+Name.ToString()+TEXT("; ");}return Pin;};
 auto Link=[&](UEdGraphPin* A,UEdGraphPin* B){if(!A||!B||!A->GetOwningNode()->GetGraph()->GetSchema()->TryCreateConnection(A,B)){OK=false;Error+=TEXT("link failed; ");}};
 auto Literal=[&](UEdGraphNode* N,FName Name,const FString& V){if(auto* Pin=P(N,Name))Pin->DefaultValue=V;};
 auto Call=[&](UEdGraph* G,FName Name){auto* F=UKismetMathLibrary::StaticClass()->FindFunctionByName(Name);check(F);FGraphNodeCreator<UK2Node_CallFunction> C(*G);auto* N=C.CreateNode();N->SetFromFunction(F);C.Finalize();return N;};
 auto Return=[&](UEdGraphNode* N){return P(N,UEdGraphSchema_K2::PN_ReturnValue);};
 auto Get=[&](UEdGraph* G,FName Name){FGraphNodeCreator<UK2Node_VariableGet> C(*G);auto* N=C.CreateNode();N->VariableReference.SetSelfMember(Name);C.Finalize();return P(N,Name);};
 auto Set=[&](FName Name){FGraphNodeCreator<UK2Node_VariableSet> C(*E);auto* N=C.CreateNode();N->VariableReference.SetSelfMember(Name);C.Finalize();return N;};
 auto Vec=[&](const TSharedPtr<FJsonObject>& J,const TCHAR* Key){auto V=J->GetArrayField(Key);return FString::Printf(TEXT("%.12f,%.12f,%.12f"),V[0]->AsNumber(),V[1]->AsNumber(),V[2]->AsNumber());};
 auto Quat=[&](const TSharedPtr<FJsonObject>& J,const TCHAR* Key){auto V=J->GetArrayField(Key);auto* N=Call(E,TEXT("MakeQuat"));int32 I=0;for(FName Component:{FName(TEXT("X")),FName(TEXT("Y")),FName(TEXT("Z")),FName(TEXT("W"))})Literal(N,Component,FString::Printf(TEXT("%.12f"),V[I++]->AsNumber()));return Return(N);};
 auto Multiply=[&](UEdGraphPin* A,UEdGraphPin* B){auto* N=Call(E,TEXT("Multiply_QuatQuat"));Link(A,P(N,TEXT("A")));Link(B,P(N,TEXT("B")));return Return(N);};
 auto Inverse=[&](UEdGraphPin* Q){auto* N=Call(E,TEXT("Quat_Inversed"));Link(Q,P(N,TEXT("Q")));return Return(N);};
 auto Rotator=[&](UEdGraphPin* Q){auto* N=Call(E,TEXT("Quat_Rotator"));Link(Q,P(N,TEXT("Q")));return Return(N);};
 UEdGraphPin *Mesh=nullptr,*Delta=nullptr,*Positive=nullptr,*Negative=nullptr;
 for(UEdGraphNode* N:E->Nodes){
  if(auto* V=Cast<UK2Node_VariableGet>(N))if(V->VariableReference.GetMemberName()==TEXT("Mesh"))Mesh=P(V,TEXT("Mesh"));
  if(auto* V=Cast<UK2Node_VariableSet>(N))if(V->VariableReference.GetMemberName()==TEXT("HcxGaitAlphaR")){if(P(V,TEXT("HcxGaitAlphaR"))->LinkedTo.Num())Positive=P(V,UEdGraphSchema_K2::PN_Then);else Negative=P(V,UEdGraphSchema_K2::PN_Then);}
  if(auto* V=Cast<UK2Node_Event>(N))if(V->EventReference.GetMemberName()==TEXT("BlueprintUpdateAnimation"))Delta=P(V,TEXT("DeltaTimeX"));
 }
 if(!Mesh||!Delta||!Positive||!Negative)return TEXT("movement event anchors missing");
 auto* Zero=Set(TEXT("HcxHipWeight"));Literal(Zero,TEXT("HcxHipWeight"),TEXT("0.0"));Link(Negative,P(Zero,UEdGraphSchema_K2::PN_Execute));
 auto* Smooth=Call(E,TEXT("FInterpTo"));Link(Get(E,TEXT("HcxHipWeight")),P(Smooth,TEXT("Current")));Literal(Smooth,TEXT("Target"),TEXT("1.0"));Literal(Smooth,TEXT("InterpSpeed"),TEXT("15.0"));Link(Delta,P(Smooth,TEXT("DeltaTime")));
 auto* Weight=Set(TEXT("HcxHipWeight"));Link(Positive,P(Weight,UEdGraphSchema_K2::PN_Execute));Link(Return(Smooth),P(Weight,TEXT("HcxHipWeight")));Positive=P(Weight,UEdGraphSchema_K2::PN_Then);
 auto Socket=[&](const FString& Bone){FGraphNodeCreator<UK2Node_CallFunction> C(*E);auto* N=C.CreateNode();N->SetFromFunction(USceneComponent::StaticClass()->FindFunctionByName(TEXT("GetSocketRotation")));C.Finalize();Link(Mesh,P(N,UEdGraphSchema_K2::PN_Self));Literal(N,TEXT("InSocketName"),Bone);auto* Q=Call(E,TEXT("Conv_RotatorToQuaternion"));Link(Return(N),P(Q,TEXT("InRot")));return Return(Q);};
 auto* PelvisInverse=Inverse(Socket(TEXT("pelvis")));
 for(bool Left:{true,false}){
  auto J=Settings->GetObjectField(Left?TEXT("L"):TEXT("R"));const FString Side=Left?TEXT("l"):TEXT("r");
  auto* Current=Multiply(Multiply(Quat(J,TEXT("pelvis_offset_inverse_quat")),Multiply(PelvisInverse,Socket(TEXT("thigh_")+Side))),Quat(J,TEXT("thigh_offset_quat")));
  auto* Change=Multiply(Quat(J,TEXT("neutral_local_inverse_quat")),Current);auto* Break=Call(E,TEXT("BreakQuat"));Link(Change,P(Break,TEXT("InQuat")));auto* Make=Call(E,TEXT("MakeVector"));for(FName Component:{FName(TEXT("X")),FName(TEXT("Y")),FName(TEXT("Z"))})Link(P(Break,Component),P(Make,Component));
  auto* Dot=Call(E,TEXT("Dot_VectorVector"));Link(Return(Make),P(Dot,TEXT("A")));Literal(Dot,TEXT("B"),Vec(J,TEXT("thigh_axis_local")));
  auto* Angle=Call(E,TEXT("DegAtan2"));Link(Return(Dot),P(Angle,TEXT("Y")));Link(P(Break,TEXT("W")),P(Angle,TEXT("X")));auto* Double=Call(E,TEXT("Multiply_DoubleDouble"));Link(Return(Angle),P(Double,TEXT("A")));Literal(Double,TEXT("B"),TEXT("2.0"));auto* Normalize=Call(E,TEXT("NormalizeAxis"));Link(Return(Double),P(Normalize,TEXT("Angle")));
  auto* Scale=Call(E,TEXT("Multiply_DoubleDouble"));Link(Return(Normalize),P(Scale,TEXT("A")));Literal(Scale,TEXT("B"),FString::SanitizeFloat(Settings->GetNumberField(TEXT("keep_twist_fraction"))-1));auto* Clamp=Call(E,TEXT("FClamp"));Link(Return(Scale),P(Clamp,TEXT("Value")));Literal(Clamp,TEXT("Min"),FString::SanitizeFloat(-Settings->GetNumberField(TEXT("max_correction_degrees"))));Literal(Clamp,TEXT("Max"),FString::SanitizeFloat(Settings->GetNumberField(TEXT("max_correction_degrees"))));
  auto* AxisAngle=Call(E,TEXT("RotatorFromAxisAndAngle"));Literal(AxisAngle,TEXT("Axis"),Vec(J,TEXT("thigh_axis_local")));Link(Return(Clamp),P(AxisAngle,TEXT("Angle")));auto* Q=Call(E,TEXT("Conv_RotatorToQuaternion"));Link(Return(AxisAngle),P(Q,TEXT("InRot")));auto* Corrected=Multiply(Current,Return(Q));
  const FName HipName=Left?TEXT("HcxHipTargetL"):TEXT("HcxHipTargetR"),RollName=Left?TEXT("HcxRollTargetL"):TEXT("HcxRollTargetR"),AngleName=Left?TEXT("HcxHipAngleL"):TEXT("HcxHipAngleR");
  for(auto Pair:{TPair<FName,UEdGraphPin*>(HipName,Rotator(Corrected)),TPair<FName,UEdGraphPin*>(RollName,Rotator(Multiply(Corrected,Quat(J,TEXT("roll_offset_quat"))))),TPair<FName,UEdGraphPin*>(AngleName,Return(Clamp))}){auto* Store=Set(Pair.Key);Link(Positive,P(Store,UEdGraphSchema_K2::PN_Execute));Link(Pair.Value,P(Store,Pair.Key));Positive=P(Store,UEdGraphSchema_K2::PN_Then);}
 }
 UEdGraph* G=nullptr;UAnimGraphNode_Root* Root=nullptr;TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);for(auto* Graph:Graphs)for(UEdGraphNode* N:Graph->Nodes)if(auto* V=Cast<UAnimGraphNode_Root>(N)){G=Graph;Root=V;}
 if(!Root||P(Root,TEXT("Result"))->LinkedTo.Num()!=1)return TEXT("root pose missing");auto* Current=P(Root,TEXT("Result"))->LinkedTo[0];P(Root,TEXT("Result"))->BreakAllPinLinks();auto Out=[](UEdGraphNode* N)->UEdGraphPin*{for(auto* Pin:N->Pins)if(Pin->Direction==EGPD_Output)return Pin;return nullptr;};
 FGraphNodeCreator<UAnimGraphNode_LocalToComponentSpace> LC(*G);auto* Local=LC.CreateNode();LC.Finalize();Link(Current,P(Local,TEXT("LocalPose")));Current=Out(Local);
 for(bool Left:{true,false})for(bool Roll:{false,true}){
  FGraphNodeCreator<UAnimGraphNode_ModifyBone> C(*G);auto* N=C.CreateNode();N->Node.BoneToModify.BoneName=Roll?(Left?TEXT("RB_L_ThighRoll"):TEXT("RB_R_ThighRoll")):(Left?TEXT("SKEL_L_Thigh"):TEXT("SKEL_R_Thigh"));N->Node.RotationMode=BMM_Replace;N->Node.RotationSpace=BCS_ParentBoneSpace;N->Node.Alpha=1;C.Finalize();
  for(FName Name:{FName(TEXT("Rotation")),FName(TEXT("Alpha"))})if(!N->FindPin(Name))for(int32 I=0;I<N->ShowPinForProperties.Num();++I)if(N->ShowPinForProperties[I].PropertyName==Name){N->SetPinVisibility(true,I);break;}
  const FName Variable=Roll?(Left?TEXT("HcxRollTargetL"):TEXT("HcxRollTargetR")):(Left?TEXT("HcxHipTargetL"):TEXT("HcxHipTargetR"));Link(Get(G,Variable),P(N,TEXT("Rotation")));Link(Get(G,TEXT("HcxHipWeight")),P(N,TEXT("Alpha")));Link(Current,P(N,TEXT("ComponentPose")));Current=Out(N);
 }
 FGraphNodeCreator<UAnimGraphNode_ComponentToLocalSpace> CL(*G);auto* Component=CL.CreateNode();CL.Finalize();Link(Current,P(Component,TEXT("ComponentPose")));Link(Out(Component),P(Root,TEXT("Result")));
 if(!OK)return Error;FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);BP->MarkPackageDirty();return BP->Status==BS_Error?TEXT("hip blueprint compilation failed"):TEXT("OK");
}
