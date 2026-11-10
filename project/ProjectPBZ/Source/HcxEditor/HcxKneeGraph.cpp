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

FString UHcxRetargetTools::BuildHingeGraph(UAnimBlueprint* BP,const FString& SettingsJson){
 TSharedPtr<FJsonObject> Settings;if(!BP||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(SettingsJson),Settings))return TEXT("missing hinge settings");
 FEdGraphPinType FloatType;FloatType.PinCategory=UEdGraphSchema_K2::PC_Real;FloatType.PinSubCategory=UEdGraphSchema_K2::PC_Float;
 for(FName Name:{FName(TEXT("HcxHingeWeight")),FName(TEXT("HcxKneeAngleL")),FName(TEXT("HcxKneeAngleR"))})FBlueprintEditorUtils::AddMemberVariable(BP,Name,FloatType,TEXT("0.0"));
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);bool OK=true;FString Error;UEdGraph* E=BP->UbergraphPages[0];
 auto P=[&](UEdGraphNode* N,FName Name)->UEdGraphPin*{auto* Pin=N?N->FindPin(Name):nullptr;if(!Pin){OK=false;Error+=TEXT("missing pin ")+Name.ToString()+TEXT("; ");}return Pin;};
 auto Link=[&](UEdGraphPin* A,UEdGraphPin* B){if(!A||!B||!A->GetOwningNode()->GetGraph()->GetSchema()->TryCreateConnection(A,B)){OK=false;Error+=TEXT("connection failed; ");}};
 auto Literal=[&](UEdGraphNode* N,FName Name,const FString& V){if(auto* Pin=P(N,Name))Pin->DefaultValue=V;};
 auto Call=[&](UEdGraph* G,FName Name){auto* F=UKismetMathLibrary::StaticClass()->FindFunctionByName(Name);check(F);FGraphNodeCreator<UK2Node_CallFunction> C(*G);auto* N=C.CreateNode();N->SetFromFunction(F);C.Finalize();return N;};
 auto Return=[&](UEdGraphNode* N){return P(N,UEdGraphSchema_K2::PN_ReturnValue);};
 auto Get=[&](UEdGraph* G,FName Name){FGraphNodeCreator<UK2Node_VariableGet> C(*G);auto* N=C.CreateNode();N->VariableReference.SetSelfMember(Name);C.Finalize();return P(N,Name);};
 auto Set=[&](FName Name){FGraphNodeCreator<UK2Node_VariableSet> C(*E);auto* N=C.CreateNode();N->VariableReference.SetSelfMember(Name);C.Finalize();return N;};
 auto Vec=[&](const TSharedPtr<FJsonObject>& J,const TCHAR* Key){auto V=J->GetArrayField(Key);return FString::Printf(TEXT("%.12f,%.12f,%.12f"),V[0]->AsNumber(),V[1]->AsNumber(),V[2]->AsNumber());};
 auto Quat=[&](UEdGraph* G,const TSharedPtr<FJsonObject>& J,const TCHAR* Key){auto V=J->GetArrayField(Key);auto* N=Call(G,TEXT("MakeQuat"));int32 I=0;for(FName Component:{FName(TEXT("X")),FName(TEXT("Y")),FName(TEXT("Z")),FName(TEXT("W"))})Literal(N,Component,FString::Printf(TEXT("%.12f"),V[I++]->AsNumber()));return Return(N);};
 UEdGraphPin *Mesh=nullptr,*Delta=nullptr,*Positive=nullptr,*Negative=nullptr;
 for(UEdGraphNode* N:E->Nodes){
  if(auto* V=Cast<UK2Node_VariableGet>(N))if(V->VariableReference.GetMemberName()==TEXT("Mesh"))Mesh=P(V,TEXT("Mesh"));
  if(auto* V=Cast<UK2Node_VariableSet>(N))if(V->VariableReference.GetMemberName()==TEXT("HcxGaitAlphaR")){if(P(V,TEXT("HcxGaitAlphaR"))->LinkedTo.Num())Positive=P(V,UEdGraphSchema_K2::PN_Then);else Negative=P(V,UEdGraphSchema_K2::PN_Then);}
  if(auto* V=Cast<UK2Node_Event>(N))if(V->EventReference.GetMemberName()==TEXT("BlueprintUpdateAnimation"))Delta=P(V,TEXT("DeltaTimeX"));
 }
 if(!Mesh||!Delta||!Positive||!Negative)return TEXT("gait anchors missing");
 auto* Zero=Set(TEXT("HcxHingeWeight"));Literal(Zero,TEXT("HcxHingeWeight"),TEXT("0.0"));Link(Negative,P(Zero,UEdGraphSchema_K2::PN_Execute));
 auto* Smooth=Call(E,TEXT("FInterpTo"));Link(Get(E,TEXT("HcxHingeWeight")),P(Smooth,TEXT("Current")));Literal(Smooth,TEXT("Target"),TEXT("1.0"));Literal(Smooth,TEXT("InterpSpeed"),TEXT("15.0"));Link(Delta,P(Smooth,TEXT("DeltaTime")));
 auto* Weight=Set(TEXT("HcxHingeWeight"));Link(Positive,P(Weight,UEdGraphSchema_K2::PN_Execute));Link(Return(Smooth),P(Weight,TEXT("HcxHingeWeight")));Positive=P(Weight,UEdGraphSchema_K2::PN_Then);
 auto SocketQuat=[&](const FString& Bone){FGraphNodeCreator<UK2Node_CallFunction> C(*E);auto* N=C.CreateNode();N->SetFromFunction(USceneComponent::StaticClass()->FindFunctionByName(TEXT("GetSocketRotation")));C.Finalize();Link(Mesh,P(N,UEdGraphSchema_K2::PN_Self));Literal(N,TEXT("InSocketName"),Bone);auto* Q=Call(E,TEXT("Conv_RotatorToQuaternion"));Link(Return(N),P(Q,TEXT("InRot")));return Return(Q);};
 for(bool Left:{true,false}){
  auto J=Settings->GetObjectField(Left?TEXT("L"):TEXT("R"));const FString Side=Left?TEXT("l"):TEXT("r");const FName Var=Left?TEXT("HcxKneeAngleL"):TEXT("HcxKneeAngleR");
  auto* Inv=Call(E,TEXT("Quat_Inversed"));Link(SocketQuat(TEXT("thigh_")+Side),P(Inv,TEXT("Q")));
  auto* Local=Call(E,TEXT("Multiply_QuatQuat"));Link(Return(Inv),P(Local,TEXT("A")));Link(SocketQuat(TEXT("calf_")+Side),P(Local,TEXT("B")));
  auto* Change=Call(E,TEXT("Multiply_QuatQuat"));Link(Quat(E,J,TEXT("source_neutral_inverse_quat")),P(Change,TEXT("A")));Link(Return(Local),P(Change,TEXT("B")));
  auto* Break=Call(E,TEXT("BreakQuat"));Link(Return(Change),P(Break,TEXT("InQuat")));auto* Make=Call(E,TEXT("MakeVector"));for(FName Component:{FName(TEXT("X")),FName(TEXT("Y")),FName(TEXT("Z"))})Link(P(Break,Component),P(Make,Component));
  auto* Dot=Call(E,TEXT("Dot_VectorVector"));Link(Return(Make),P(Dot,TEXT("A")));Literal(Dot,TEXT("B"),Vec(J,TEXT("source_hinge_axis")));
  auto* Angle=Call(E,TEXT("DegAtan2"));Link(Return(Dot),P(Angle,TEXT("Y")));Link(P(Break,TEXT("W")),P(Angle,TEXT("X")));
  auto* Double=Call(E,TEXT("Multiply_DoubleDouble"));Link(Return(Angle),P(Double,TEXT("A")));Literal(Double,TEXT("B"),TEXT("2.0"));auto* Normalize=Call(E,TEXT("NormalizeAxis"));Link(Return(Double),P(Normalize,TEXT("Angle")));
  auto* Store=Set(Var);Link(Positive,P(Store,UEdGraphSchema_K2::PN_Execute));Link(Return(Normalize),P(Store,Var));Positive=P(Store,UEdGraphSchema_K2::PN_Then);
 }
 UEdGraph* G=nullptr;UAnimGraphNode_Root* Root=nullptr;TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);for(auto* Graph:Graphs)for(UEdGraphNode* N:Graph->Nodes)if(auto* V=Cast<UAnimGraphNode_Root>(N)){G=Graph;Root=V;}
 if(!Root||P(Root,TEXT("Result"))->LinkedTo.Num()!=1)return TEXT("root pose missing");auto* Current=P(Root,TEXT("Result"))->LinkedTo[0];P(Root,TEXT("Result"))->BreakAllPinLinks();
 auto Out=[](UEdGraphNode* N)->UEdGraphPin*{for(auto* Pin:N->Pins)if(Pin->Direction==EGPD_Output)return Pin;return nullptr;};
 FGraphNodeCreator<UAnimGraphNode_LocalToComponentSpace> LC(*G);auto* Local=LC.CreateNode();LC.Finalize();Link(Current,P(Local,TEXT("LocalPose")));Current=Out(Local);
 for(bool Left:{true,false}){
  auto J=Settings->GetObjectField(Left?TEXT("L"):TEXT("R"));
  FGraphNodeCreator<UAnimGraphNode_ModifyBone> C(*G);auto* N=C.CreateNode();N->Node.BoneToModify.BoneName=Left?TEXT("SKEL_L_Calf"):TEXT("SKEL_R_Calf");N->Node.RotationMode=BMM_Replace;N->Node.RotationSpace=BCS_ParentBoneSpace;N->Node.Alpha=1;C.Finalize();
  for(FName Name:{FName(TEXT("Rotation")),FName(TEXT("Alpha"))})if(!N->FindPin(Name))for(int32 I=0;I<N->ShowPinForProperties.Num();++I)if(N->ShowPinForProperties[I].PropertyName==Name){N->SetPinVisibility(true,I);break;}
  auto* AxisAngle=Call(G,TEXT("RotatorFromAxisAndAngle"));Literal(AxisAngle,TEXT("Axis"),Vec(J,TEXT("target_hinge_axis")));Link(Get(G,Left?TEXT("HcxKneeAngleL"):TEXT("HcxKneeAngleR")),P(AxisAngle,TEXT("Angle")));
  auto* Q=Call(G,TEXT("Conv_RotatorToQuaternion"));Link(Return(AxisAngle),P(Q,TEXT("InRot")));auto* Product=Call(G,TEXT("Multiply_QuatQuat"));Link(Quat(G,J,TEXT("target_neutral_quat")),P(Product,TEXT("A")));Link(Return(Q),P(Product,TEXT("B")));
  auto* Rotator=Call(G,TEXT("Quat_Rotator"));Link(Return(Product),P(Rotator,TEXT("Q")));Link(Return(Rotator),P(N,TEXT("Rotation")));Link(Get(G,TEXT("HcxHingeWeight")),P(N,TEXT("Alpha")));Link(Current,P(N,TEXT("ComponentPose")));Current=Out(N);
 }
 FGraphNodeCreator<UAnimGraphNode_ComponentToLocalSpace> CL(*G);auto* Component=CL.CreateNode();CL.Finalize();Link(Current,P(Component,TEXT("ComponentPose")));Link(Out(Component),P(Root,TEXT("Result")));
 if(!OK)return Error;FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);BP->MarkPackageDirty();return BP->Status==BS_Error?TEXT("hinge blueprint compilation failed"):TEXT("OK");
}
