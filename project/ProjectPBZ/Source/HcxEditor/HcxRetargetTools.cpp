#include "HcxRetargetTools.h"
#include "Modules/ModuleManager.h"
#include "Animation/AnimBlueprint.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_RetargetPoseFromMesh.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UnrealType.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "Retargeter/IKRetargetProcessor.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Editor.h"
#include "Engine/World.h"
#include "PhysicsEngine/PhysicsAsset.h"
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultModuleImpl, HcxEditor, "HcxEditor");
bool UHcxRetargetTools::RestorePhysicsReference(USkeletalMesh* Mesh,const FString& AssetPackage) {
 if(!Mesh)return false;
 UPackage* Package=CreatePackage(*AssetPackage);FString Name=FPackageName::GetLongPackageAssetName(AssetPackage);
 UPhysicsAsset* Asset=FindObject<UPhysicsAsset>(Package,*Name);
 if(!Asset)Asset=NewObject<UPhysicsAsset>(Package,*Name,RF_Public|RF_Standalone);
 Mesh->SetPhysicsAsset(Asset);Mesh->MarkPackageDirty();Asset->MarkPackageDirty();return true;
}
AActor* UHcxRetargetTools::SpawnPreviewActor(TSubclassOf<AActor> ActorClass,const FTransform& Transform) {
 UWorld* World=GEditor?GEditor->GetEditorWorldContext().World():nullptr;
 if(!World||!ActorClass)return nullptr;
 FActorSpawnParameters Params;Params.ObjectFlags=RF_Transactional;
 return World->SpawnActor(ActorClass.Get(),&Transform,Params);
}
bool UHcxRetargetTools::BuildRetargetGraph(UAnimBlueprint* Blueprint, UIKRetargeter* Retargeter) {
 if (!Blueprint || !Retargeter) return false;
 TArray<UEdGraph*> Graphs; Blueprint->GetAllGraphs(Graphs);
 for (UEdGraph* Graph : Graphs) {
  UAnimGraphNode_Root* Root=nullptr;
  for (UEdGraphNode* Node : Graph->Nodes) if (auto* R=Cast<UAnimGraphNode_Root>(Node)) { Root=R; break; }
  if (!Root) continue;
  FGraphNodeCreator<UAnimGraphNode_RetargetPoseFromMesh> Creator(*Graph);
  auto* Node=Creator.CreateNode();
  auto* NodeProperty=FindFProperty<FStructProperty>(Node->GetClass(),TEXT("Node"));
  if (!NodeProperty) return false;
  auto* Data=NodeProperty->ContainerPtrToValuePtr<FAnimNode_RetargetPoseFromMesh>(Node);
  Data->IKRetargeterAsset=Retargeter;
  Data->RetargetFrom=ERetargetSourceMode::ParentSkeletalMeshComponent;
  Creator.Finalize();
  UEdGraphPin* Out=nullptr; UEdGraphPin* In=nullptr;
  for (auto* Pin : Node->Pins) if (Pin->Direction==EGPD_Output) { Out=Pin; break; }
  for (auto* Pin : Root->Pins) if (Pin->Direction==EGPD_Input) { In=Pin; break; }
  if (!Out || !In || !Graph->GetSchema()->TryCreateConnection(Out,In)) return false;
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
  FKismetEditorUtilities::CompileBlueprint(Blueprint);
  return Blueprint->Status!=BS_Error;
 }
 return false;
}

bool UHcxRetargetTools::BakeDiagnosticAnimation(USkeletalMesh* Source, USkeletalMesh* Target, UIKRetargeter* Retargeter, const FString& InputFile, const FString& AssetDirectory, const FString& OutputFile) {
 if (!Source || !Target || !Retargeter) return false;
 FString Text; TSharedPtr<FJsonObject> Input;
 if (!FFileHelper::LoadFileToString(Text,*InputFile) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Input)) return false;
 const auto& SourceRef=Source->GetRefSkeleton(); const auto& TargetRef=Target->GetRefSkeleton();
 const auto& Names=Input->GetArrayField(TEXT("bones")); const auto& Frames=Input->GetArrayField(TEXT("frames"));
 const int32 NumSamples=Frames.Num(); if (NumSamples<2) return false;
 TArray<int32> Map;
 for (const auto& N : Names) Map.Add(SourceRef.FindBoneIndex(FName(N->AsString())));
 for (int32 I : Map) if (I<0) return false;
 auto GlobalPose=[](const TArray<FTransform>& Local,const FReferenceSkeleton& Ref) {
  auto Global=Local;
  for (int32 I=0; I<Global.Num(); ++I) { int32 P=Ref.GetParentIndex(I); if(P>=0) Global[I]=Local[I]*Global[P]; }
  return Global;
 };
 auto JsonPose=[](const TArray<FTransform>& Pose) {
  TArray<TSharedPtr<FJsonValue>> Result;
  for (const auto& T : Pose) {
   const FVector P=T.GetLocation(), S=T.GetScale3D();const FQuat Q=T.GetRotation();
   TArray<TSharedPtr<FJsonValue>> A; for (double X : {P.X,P.Y,P.Z,Q.X,Q.Y,Q.Z,Q.W,S.X,S.Y,S.Z}) A.Add(MakeShared<FJsonValueNumber>(X));
   Result.Add(MakeShared<FJsonValueArray>(A));
  }
  return Result;
 };
 auto JsonSkeleton=[&](const FReferenceSkeleton& Ref) {
  auto Result=MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> N,P;
  for(int32 I=0;I<Ref.GetNum();++I){N.Add(MakeShared<FJsonValueString>(Ref.GetBoneName(I).ToString()));P.Add(MakeShared<FJsonValueNumber>(Ref.GetParentIndex(I)));}
  Result->SetArrayField(TEXT("bones"),N);Result->SetArrayField(TEXT("parents"),P);
  Result->SetArrayField(TEXT("reference_global"),JsonPose(GlobalPose(Ref.GetRefBonePose(),Ref)));return Result;
 };
 FIKRetargetProcessor Processor; FRetargetProfile Profile; Processor.Initialize(Source,Target,Retargeter,Profile,false);
 if(!Processor.IsInitialized())return false;
 TArray<TArray<FTransform>> SourceLocalFrames,TargetLocalFrames;
 TArray<TSharedPtr<FJsonValue>> SourceJson,TargetJson;
 const double Duration=Input->GetNumberField(TEXT("duration"));
 for(int32 F=0;F<NumSamples;++F){
  auto Local=SourceRef.GetRefBonePose();const auto& Frame=Frames[F]->AsArray();
  if(Frame.Num()!=Map.Num())return false;
  for(int32 B=0;B<Map.Num();++B){
   const auto& V=Frame[B]->AsArray();if(V.Num()!=10)return false;
   FQuat Q(V[3]->AsNumber(),V[4]->AsNumber(),V[5]->AsNumber(),V[6]->AsNumber());Q.Normalize();
   Local[Map[B]]=FTransform(Q,FVector(V[0]->AsNumber(),V[1]->AsNumber(),V[2]->AsNumber()),FVector(V[7]->AsNumber(),V[8]->AsNumber(),V[9]->AsNumber()));
  }
  // These diagnostic clips enable root motion. In game the movement component consumes it;
  // the skeletal pose driving the child retargeter has its root locked back to reference.
  Local[0]=SourceRef.GetRefBonePose()[0];
  auto Global=GlobalPose(Local,SourceRef);auto Scaled=Global;Processor.ApplySourceScaleToPose(Scaled);
  TArray<FTransform> TargetGlobal=Processor.RunRetargeter(Scaled,Profile,Duration/(NumSamples-1));
  if(TargetGlobal.Num()!=TargetRef.GetNum())return false;
  TArray<FTransform> TargetLocal=TargetGlobal;
  for(int32 B=0;B<TargetGlobal.Num();++B){
   if(TargetGlobal[B].ContainsNaN())return false;
   int32 P=TargetRef.GetParentIndex(B);if(P>=0)TargetLocal[B]=TargetGlobal[B].GetRelativeTransform(TargetGlobal[P]);
  }
  SourceLocalFrames.Add(Local);TargetLocalFrames.Add(TargetLocal);
  SourceJson.Add(MakeShared<FJsonValueArray>(JsonPose(Global)));TargetJson.Add(MakeShared<FJsonValueArray>(JsonPose(TargetGlobal)));
 }
 const FString Clip=Input->GetStringField(TEXT("name"));
 auto CreateSequence=[&](const FString& Name,USkeletalMesh* Mesh,const TArray<TArray<FTransform>>& Poses){
  FString PackageName=AssetDirectory/TEXT("A_")+Name;UPackage* Package=CreatePackage(*PackageName);
  UAnimSequence* Sequence=FindObject<UAnimSequence>(Package,*FPackageName::GetLongPackageAssetName(PackageName));
  if(!Sequence)Sequence=NewObject<UAnimSequence>(Package,*FPackageName::GetLongPackageAssetName(PackageName),RF_Public|RF_Standalone|RF_Transactional);
  Sequence->SetSkeleton(Mesh->GetSkeleton());Sequence->SetPreviewMesh(Mesh);
  auto& Controller=Sequence->GetController();Controller.InitializeModel();Controller.OpenBracket(FText::FromString(TEXT("Import local diagnostic poses")),false);
  // These extracted clips are exactly 30 or 60 Hz. Keep the rational reduced to
  // avoid large intermediate products in editor thumbnail/timeline conversions.
  const int32 Rate=FMath::RoundToInt((NumSamples-1)/Duration);Controller.SetFrameRate(FFrameRate(Rate,1),false);Controller.SetNumberOfFrames(FFrameNumber(NumSamples-1),false);
  const auto& Ref=Mesh->GetRefSkeleton();
  for(int32 B=0;B<Ref.GetNum();++B){
   TArray<FVector3f> Positions,Scales;TArray<FQuat4f> Rotations;
   for(const auto& Pose:Poses){Positions.Add(FVector3f(Pose[B].GetLocation()));Rotations.Add(FQuat4f(Pose[B].GetRotation()));Scales.Add(FVector3f(Pose[B].GetScale3D()));}
   Controller.AddBoneCurve(Ref.GetBoneName(B),false);
   if(!Controller.SetBoneTrackKeys(Ref.GetBoneName(B),Positions,Rotations,Scales,false))return false;
  }
  Controller.NotifyPopulated();Controller.CloseBracket(false);Sequence->MarkPackageDirty();return true;
 };
 if(!CreateSequence(Clip+TEXT("_Soul"),Source,SourceLocalFrames)||!CreateSequence(Clip+TEXT("_2B_Baseline"),Target,TargetLocalFrames))return false;
 auto Result=MakeShared<FJsonObject>();Result->SetStringField(TEXT("name"),Clip);Result->SetBoolField(TEXT("root_motion_removed"),true);Result->SetNumberField(TEXT("duration"),Duration);Result->SetNumberField(TEXT("fps"),(NumSamples-1)/Duration);
 auto S=JsonSkeleton(SourceRef);S->SetArrayField(TEXT("frames"),SourceJson);Result->SetObjectField(TEXT("source"),S);
 auto T=JsonSkeleton(TargetRef);T->SetArrayField(TEXT("frames"),TargetJson);Result->SetObjectField(TEXT("target"),T);
 FString Json;FJsonSerializer::Serialize(Result,TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json));
 return FFileHelper::SaveStringToFile(Json,*OutputFile,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void UHcxRetargetTools::SyncReferencePose(USkeletalMesh* Mesh) {
 if (Mesh && Mesh->GetSkeleton()) { Mesh->GetSkeleton()->UpdateReferencePoseFromMesh(Mesh); Mesh->GetSkeleton()->MarkPackageDirty(); }
}
FString UHcxRetargetTools::CheckRetargetMotion(USkeletalMesh* Source, USkeletalMesh* Target, UIKRetargeter* Retargeter) {
 if (!Source || !Target || !Retargeter) return TEXT("{\"ok\":false,\"reason\":\"missing asset\"}");
 FIKRetargetProcessor Processor; FRetargetProfile Profile;
 Processor.Initialize(Source,Target,Retargeter,Profile,false);
 if (!Processor.IsInitialized()) return TEXT("{\"ok\":false,\"reason\":\"processor initialization failed\"}");
 const auto& Ref=Source->GetRefSkeleton();
 auto GlobalPose=[&](const TArray<FTransform>& Local) {
  TArray<FTransform> Global=Local;
  for (int32 I=0; I<Global.Num(); ++I) { int32 P=Ref.GetParentIndex(I); if (P>=0) Global[I]=Local[I]*Global[P]; }
  Processor.ApplySourceScaleToPose(Global); return Global;
 };
 auto BaseInput=GlobalPose(Ref.GetRefBonePose());
 const TArray<FTransform> Baseline=Processor.RunRetargeter(BaseInput,Profile,1.f/60.f);
 FString Cases; bool Success=true;
 for (const TCHAR* Bone : {TEXT("upperarm_l"),TEXT("thigh_l"),TEXT("spine_01")}) {
  int32 Index=Ref.FindBoneIndex(FName(Bone)); if (Index<0) return TEXT("{\"ok\":false,\"reason\":\"missing source bone\"}");
  auto Local=Ref.GetRefBonePose(); Local[Index].SetRotation(FQuat(FVector::UpVector,FMath::DegreesToRadians(25.f))*Local[Index].GetRotation());
  auto Input=GlobalPose(Local); const auto& Output=Processor.RunRetargeter(Input,Profile,1.f/60.f);
  double Distance=0, Angle=0;
  if (Output.Num()!=Target->GetRefSkeleton().GetNum()) Success=false;
  for (int32 I=0; I<FMath::Min(Output.Num(),Baseline.Num()); ++I) {
   if (Output[I].ContainsNaN()) Success=false;
   Distance=FMath::Max(Distance,FVector::Distance(Output[I].GetLocation(),Baseline[I].GetLocation()));
   Angle=FMath::Max(Angle,double(Output[I].GetRotation().AngularDistance(Baseline[I].GetRotation())));
  }
  Success &= Distance>0.001 || Angle>0.001;
  if (!Cases.IsEmpty()) Cases+=TEXT(",");
  Cases+=FString::Printf(TEXT("{\"bone\":\"%s\",\"max_movement_cm\":%.6f,\"max_angle_radians\":%.6f}"),Bone,Distance,Angle);
 }
 int32 Head=Target->GetRefSkeleton().FindBoneIndex(TEXT("SKEL_Head"));
 int32 Pelvis=Target->GetRefSkeleton().FindBoneIndex(TEXT("SKEL_Pelvis"));
 return FString::Printf(TEXT("{\"ok\":%s,\"target_bones\":%d,\"baseline_root_z\":%.6f,\"baseline_pelvis_z\":%.6f,\"baseline_head_z\":%.6f,\"cases\":[%s]}"),Success?TEXT("true"):TEXT("false"),Baseline.Num(),Baseline[0].GetLocation().Z,Baseline[Pelvis].GetLocation().Z,Baseline[Head].GetLocation().Z,*Cases);
}
