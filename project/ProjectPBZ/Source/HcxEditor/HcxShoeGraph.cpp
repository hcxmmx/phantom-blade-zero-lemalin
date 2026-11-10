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

FString UHcxRetargetTools::BuildShoeGraph(UAnimBlueprint* BP,const FString& SettingsJson) {
 TSharedPtr<FJsonObject> Settings;if(!BP||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(SettingsJson),Settings))return TEXT("missing settings");
 double MaxAngle=6,Gain=.6,NearFull=4,NearFade=10,InterpSpeed=12;
 Settings->TryGetNumberField(TEXT("max_correction_degrees"),MaxAngle);Settings->TryGetNumberField(TEXT("correction_gain"),Gain);Settings->TryGetNumberField(TEXT("near_full_cm"),NearFull);Settings->TryGetNumberField(TEXT("near_fade_cm"),NearFade);Settings->TryGetNumberField(TEXT("interp_speed"),InterpSpeed);
 FEdGraphPinType FloatType;FloatType.PinCategory=UEdGraphSchema_K2::PC_Real;FloatType.PinSubCategory=UEdGraphSchema_K2::PC_Float;
 for(FName Name:{FName(TEXT("HcxShoeRollL")),FName(TEXT("HcxShoeRollR"))})FBlueprintEditorUtils::AddMemberVariable(BP,Name,FloatType,TEXT("0.0"));
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
 bool OK=true;FString Error;UEdGraph* E=BP->UbergraphPages[0];
 auto P=[&](UEdGraphNode* N,FName Name)->UEdGraphPin* {auto* Pin=N?N->FindPin(Name):nullptr;if(!Pin){OK=false;Error+=TEXT("missing ")+Name.ToString()+TEXT("; ");}return Pin;};
 auto Link=[&](UEdGraphPin* A,UEdGraphPin* B){if(!A||!B||!A->GetOwningNode()->GetGraph()->GetSchema()->TryCreateConnection(A,B)){OK=false;Error+=TEXT("link failed; ");}};
 auto Literal=[&](UEdGraphNode* N,FName Name,const FString& V){if(auto* Pin=P(N,Name))Pin->DefaultValue=V;};
 auto Call=[&](UEdGraph* G,FName Name){FGraphNodeCreator<UK2Node_CallFunction> C(*G);auto* N=C.CreateNode();N->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(Name));C.Finalize();return N;};
 auto Return=[&](UEdGraphNode* N){return P(N,UEdGraphSchema_K2::PN_ReturnValue);};
 auto Get=[&](UEdGraph* G,FName Name){FGraphNodeCreator<UK2Node_VariableGet> C(*G);auto* N=C.CreateNode();N->VariableReference.SetSelfMember(Name);C.Finalize();return P(N,Name);};
 auto Set=[&](FName Name){FGraphNodeCreator<UK2Node_VariableSet> C(*E);auto* N=C.CreateNode();N->VariableReference.SetSelfMember(Name);C.Finalize();return N;};
 auto Vec=[&](const TSharedPtr<FJsonObject>& J,const TCHAR* Key){auto V=J->GetArrayField(Key);return FString::Printf(TEXT("%0.12f,%0.12f,%0.12f"),V[0]->AsNumber(),V[1]->AsNumber(),V[2]->AsNumber());};
 auto Binary=[&](FName Fn,UEdGraphPin* A,UEdGraphPin* B){auto* N=Call(E,Fn);Link(A,P(N,TEXT("A")));Link(B,P(N,TEXT("B")));return Return(N);};
 auto Scalar=[&](FName Fn,UEdGraphPin* A,const TCHAR* B){auto* N=Call(E,Fn);Link(A,P(N,TEXT("A")));Literal(N,TEXT("B"),B);return Return(N);};
 auto Clamp=[&](UEdGraphPin* Value,const TCHAR* Min,const TCHAR* Max){auto* N=Call(E,TEXT("FClamp"));Link(Value,P(N,TEXT("Value")));Literal(N,TEXT("Min"),Min);Literal(N,TEXT("Max"),Max);return Return(N);};
 auto Dot=[&](UEdGraphPin* A,UEdGraphPin* B){return Binary(TEXT("Dot_VectorVector"),A,B);};
 auto Z=[&](UEdGraphPin* V){auto* N=Call(E,TEXT("BreakVector"));Link(V,P(N,TEXT("InVec")));return P(N,TEXT("Z"));};
 UEdGraphPin *Mesh=nullptr,*Delta=nullptr,*PositiveExec=nullptr,*NegativeExec=nullptr;
 for(UEdGraphNode* N:E->Nodes){
  if(auto* V=Cast<UK2Node_VariableGet>(N))if(V->VariableReference.GetMemberName()==TEXT("Mesh"))Mesh=P(V,TEXT("Mesh"));
  if(auto* V=Cast<UK2Node_VariableSet>(N))if(V->VariableReference.GetMemberName()==TEXT("HcxGaitAlphaR")){
   if(P(V,TEXT("HcxGaitAlphaR"))->LinkedTo.Num())PositiveExec=P(V,UEdGraphSchema_K2::PN_Then);else NegativeExec=P(V,UEdGraphSchema_K2::PN_Then);
  }
  if(auto* V=Cast<UK2Node_Event>(N))if(V->EventReference.GetMemberName()==TEXT("BlueprintUpdateAnimation"))Delta=P(V,TEXT("DeltaTimeX"));
 }
 if(!Mesh||!Delta||!PositiveExec||!NegativeExec)return TEXT("gait event anchors missing");
 auto Socket=[&](FName Function,const FString& Bone){FGraphNodeCreator<UK2Node_CallFunction> C(*E);auto* N=C.CreateNode();N->SetFromFunction(USceneComponent::StaticClass()->FindFunctionByName(Function));C.Finalize();Link(Mesh,P(N,UEdGraphSchema_K2::PN_Self));Literal(N,TEXT("InSocketName"),Bone);return Return(N);};
 for(bool Left:{true,false}){
  auto J=Settings->GetObjectField(Left?TEXT("L"):TEXT("R"));const FString Side=Left?TEXT("l"):TEXT("r");const FName Variable=Left?TEXT("HcxShoeRollL"):TEXT("HcxShoeRollR");
  auto* Zero=Set(Variable);Literal(Zero,Variable,TEXT("0.0"));Link(NegativeExec,P(Zero,UEdGraphSchema_K2::PN_Execute));NegativeExec=P(Zero,UEdGraphSchema_K2::PN_Then);
  auto* Rot=Socket(TEXT("GetSocketRotation"),TEXT("foot_")+Side);
  auto Rotated=[&](const TCHAR* Key){auto* N=Call(E,TEXT("GreaterGreater_VectorRotator"));Literal(N,TEXT("A"),Vec(J,Key));Link(Rot,P(N,TEXT("B")));return Return(N);};
  auto* Forward=Rotated(TEXT("source_forward_local"));auto* Up=Rotated(TEXT("source_up_local"));
  auto* Projection=Call(E,TEXT("Multiply_VectorFloat"));Link(Forward,P(Projection,TEXT("A")));Link(Z(Forward),P(Projection,TEXT("B")));
  auto* Sub=Call(E,TEXT("Subtract_VectorVector"));Literal(Sub,TEXT("A"),TEXT("0,0,1"));Link(Return(Projection),P(Sub,TEXT("B")));
  auto* Normal=Call(E,TEXT("Normal"));Link(Return(Sub),P(Normal,TEXT("A")));auto* Desired=Return(Normal);
  auto* Angle=Call(E,TEXT("DegAtan2"));Link(Dot(Forward,Binary(TEXT("Cross_VectorVector"),Up,Desired)),P(Angle,TEXT("Y")));Link(Dot(Up,Desired),P(Angle,TEXT("X")));
  auto* Bounded=Clamp(Scalar(TEXT("Multiply_DoubleDouble"),Return(Angle),*FString::SanitizeFloat(Gain)),*FString::SanitizeFloat(-MaxAngle),*FString::SanitizeFloat(MaxAngle));
  auto* Height=Z(Binary(TEXT("Subtract_VectorVector"),Socket(TEXT("GetSocketLocation"),TEXT("ball_")+Side),Socket(TEXT("GetSocketLocation"),TEXT("root"))));
  auto* Margin=Call(E,TEXT("Subtract_DoubleDouble"));Literal(Margin,TEXT("A"),FString::SanitizeFloat(NearFade));Link(Height,P(Margin,TEXT("B")));
  auto* Contact=Clamp(Scalar(TEXT("Divide_DoubleDouble"),Return(Margin),*FString::SanitizeFloat(NearFade-NearFull)),TEXT("0.0"),TEXT("1.0"));
  auto* Horizontal=Call(E,TEXT("VSizeXY"));Link(Forward,P(Horizontal,TEXT("A")));
  auto* Safe=Clamp(Scalar(TEXT("Divide_DoubleDouble"),Scalar(TEXT("Subtract_DoubleDouble"),Return(Horizontal),TEXT("0.35")),TEXT("0.35")),TEXT("0.0"),TEXT("1.0"));
  auto* Target=Binary(TEXT("Multiply_DoubleDouble"),Bounded,Binary(TEXT("Multiply_DoubleDouble"),Contact,Safe));
  auto* Smooth=Call(E,TEXT("FInterpTo"));Link(Get(E,Variable),P(Smooth,TEXT("Current")));Link(Target,P(Smooth,TEXT("Target")));Link(Delta,P(Smooth,TEXT("DeltaTime")));Literal(Smooth,TEXT("InterpSpeed"),FString::SanitizeFloat(InterpSpeed));
  auto* Store=Set(Variable);Link(PositiveExec,P(Store,UEdGraphSchema_K2::PN_Execute));Link(Return(Smooth),P(Store,Variable));PositiveExec=P(Store,UEdGraphSchema_K2::PN_Then);
 }
 UEdGraph* G=nullptr;UAnimGraphNode_Root* Root=nullptr;TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
 for(auto* Graph:Graphs)for(UEdGraphNode* N:Graph->Nodes)if(auto* R=Cast<UAnimGraphNode_Root>(N)){G=Graph;Root=R;}
 if(!Root||P(Root,TEXT("Result"))->LinkedTo.Num()!=1)return TEXT("animation root missing");
 auto* Current=P(Root,TEXT("Result"))->LinkedTo[0];P(Root,TEXT("Result"))->BreakAllPinLinks();
 auto Out=[](UEdGraphNode* N)->UEdGraphPin*{for(auto* Pin:N->Pins)if(Pin->Direction==EGPD_Output)return Pin;return nullptr;};
 FGraphNodeCreator<UAnimGraphNode_LocalToComponentSpace> LC(*G);auto* Local=LC.CreateNode();LC.Finalize();Link(Current,P(Local,TEXT("LocalPose")));Current=Out(Local);
 for(bool Left:{true,false}){
  auto J=Settings->GetObjectField(Left?TEXT("L"):TEXT("R"));
  FGraphNodeCreator<UAnimGraphNode_ModifyBone> C(*G);auto* N=C.CreateNode();N->Node.BoneToModify.BoneName=Left?TEXT("SKEL_L_Foot"):TEXT("SKEL_R_Foot");N->Node.RotationMode=BMM_Additive;N->Node.RotationSpace=BCS_BoneSpace;N->Node.Alpha=1.f;C.Finalize();
  if(!N->FindPin(TEXT("Rotation")))for(int32 Index=0;Index<N->ShowPinForProperties.Num();++Index)if(N->ShowPinForProperties[Index].PropertyName==TEXT("Rotation")){N->SetPinVisibility(true,Index);break;}
  auto* Rotation=Call(G,TEXT("RotatorFromAxisAndAngle"));Literal(Rotation,TEXT("Axis"),Vec(J,TEXT("target_forward_local")));Link(Get(G,Left?TEXT("HcxShoeRollL"):TEXT("HcxShoeRollR")),P(Rotation,TEXT("Angle")));
  Link(Return(Rotation),P(N,TEXT("Rotation")));Link(Current,P(N,TEXT("ComponentPose")));Current=Out(N);
 }
 FGraphNodeCreator<UAnimGraphNode_ComponentToLocalSpace> CL(*G);auto* Component=CL.CreateNode();CL.Finalize();Link(Current,P(Component,TEXT("ComponentPose")));Link(Out(Component),P(Root,TEXT("Result")));
 if(!OK)return Error;
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);BP->MarkPackageDirty();return BP->Status==BS_Error?TEXT("shoe blueprint compilation failed"):TEXT("OK");
}
