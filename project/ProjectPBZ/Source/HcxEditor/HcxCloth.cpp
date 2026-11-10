#include "HcxRetargetTools.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "ClothingAssetFactory.h"
#include "ClothingAsset.h"
#include "ClothLODData.h"
#include "ChaosCloth/ChaosClothConfig.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "SkinnedAssetCompiler.h"
#include "Engine/SkinnedAssetCommon.h"

static FString HcxRepairSavedClothBinding(USkeletalMesh* Mesh){
 if(Mesh->GetMeshClothingAssets().Num()!=1)return TEXT("expected one existing cloth asset");auto* Cloth=Cast<UClothingAssetCommon>(Mesh->GetMeshClothingAssets()[0]);if(!Cloth)return TEXT("existing cloth type mismatch");int32 SectionIndex=INDEX_NONE;const auto& LOD=Mesh->GetImportedModel()->LODModels[0];for(int32 I=0;I<LOD.Sections.Num();++I){int32 M=LOD.Sections[I].MaterialIndex;if(Mesh->GetMaterials().IsValidIndex(M)&&Mesh->GetMaterials()[M].MaterialSlotName.ToString().Contains(TEXT("teef_diff_000_a_uni")))SectionIndex=I;}if(SectionIndex==INDEX_NONE)return TEXT("saved skirt section missing");
 for(auto& L:Cloth->LodData){auto& P=L.PhysicalMeshData;P.CalculateNumInfluences();for(const auto& B:P.BoneData)if(B.NumInfluences==0)return TEXT("cloth vertex has no bone influences after calculation");}
 {
  // Match FSkeletalMeshEditor::ApplyClothing, including persistent source-section data.
  FScopedSkeletalMeshPostEditChange Scoped(Mesh);Mesh->Modify();Cloth->Modify();Cloth->UnbindFromSkeletalMesh(Mesh,0);
  if(!Cloth->BindToSkeletalMesh(Mesh,0,SectionIndex,0))return TEXT("saved skirt binding failed");
  auto& UpdatedLOD=Mesh->GetImportedModel()->LODModels[0];auto& Source=UpdatedLOD.UserSectionsData.FindOrAdd(UpdatedLOD.Sections[SectionIndex].OriginalDataSectionIndex);Source.CorrespondClothAssetIndex=0;Source.ClothingData.AssetGuid=Cloth->GetAssetGuid();Source.ClothingData.AssetLodIndex=0;
 }
 FSkinnedAssetCompilingManager::Get().FinishCompilation({Mesh});TArray<UClothingAssetBase*> Used;Mesh->GetClothingAssetsInUse(Used);if(!Used.Contains(Cloth)||!Mesh->HasActiveClothingAssets()||Cloth->LodMap.Num()!=1||Cloth->LodMap[0]!=0)return TEXT("cloth binding did not survive render rebuild");Mesh->MarkPackageDirty();return FString::Printf(TEXT("OK section=%d source-data saved; active cloth=%d lodmap=%d"),SectionIndex,Used.Num(),Cloth->LodMap[0]);
}

FString UHcxRetargetTools::RefineSkirtCloth(USkeletalMesh* Mesh){
 if(!Mesh||Mesh->GetMeshClothingAssets().Num()!=1)return TEXT("cloth refinement mesh missing");auto* Cloth=Cast<UClothingAssetCommon>(Mesh->GetMeshClothingAssets()[0]);if(!Cloth)return TEXT("cloth refinement type missing");auto* Config=Cloth->GetClothConfig<UChaosClothConfig>();UPhysicsAsset* PA=Cloth->PhysicsAsset;if(!Config||!PA)return TEXT("cloth config/collider missing");int32 Count=0;
 const auto& Ref=Mesh->GetRefSkeleton();TArray<FTransform> Global=Ref.GetRefBonePose();for(int32 I=0;I<Global.Num();++I){int32 P=Ref.GetParentIndex(I);if(P>=0)Global[I]*=Global[P];}int32 Pelvis=Ref.FindBoneIndex(TEXT("SKEL_Pelvis")),Left=Ref.FindBoneIndex(TEXT("SKEL_L_Thigh")),Right=Ref.FindBoneIndex(TEXT("SKEL_R_Thigh")),Back=Ref.FindBoneIndex(TEXT("SM_M_BackSkirtRoll")),Front=Ref.FindBoneIndex(TEXT("SM_M_FrontSkirtRoll"));if(FMath::Min(FMath::Min(Pelvis,Left),FMath::Min(Right,FMath::Min(Back,Front)))<0)return TEXT("hip fit references missing");FVector Rear=Global[Back].GetTranslation()-Global[Front].GetTranslation();Rear.Z=0;Rear.Normalize();FVector Side=Global[Left].GetTranslation()-Global[Right].GetTranslation();Side.Z=0;Side.Normalize();FVector LocalSide=Global[Pelvis].GetRotation().UnrotateVector(Side);
 for(USkeletalBodySetup* Body:PA->SkeletalBodySetups)if(Body&&Body->BoneName==TEXT("SKEL_Pelvis")){Body->AggGeom.SphereElems.Reset();Body->AggGeom.SphylElems.Reset();for(int32 TI:{Left,Right}){FKSphylElem C;C.Center=Global[Pelvis].InverseTransformPosition(Global[TI].GetTranslation()+Rear*5.5+FVector(0,0,1));C.Radius=6.8f;C.Length=4;C.Rotation=FQuat::FindBetweenNormals(FVector::UpVector,LocalSide).Rotator();Body->AggGeom.SphylElems.Add(C);++Count;}Body->InvalidatePhysicsData();Body->CreatePhysicsMeshes();}
 if(Count!=2)return TEXT("expected two fitted hip capsules");PA->MarkPackageDirty();Config->CollisionThickness=1.f;Config->FrictionCoefficient=.15f;Config->bUseSelfCollisions=false;Config->SelfCollisionThickness=.5f;Config->SelfCollisionFriction=.1f;Config->AnimDriveStiffness.Low=Config->AnimDriveStiffness.High=.45f;Config->BendingStiffnessWeighted.Low=Config->BendingStiffnessWeighted.High=.7f;Config->DampingCoefficient=.35f;Config->LocalDampingCoefficient=.25f;Config->GravityScale=1;Config->LinearVelocityScale=FVector(.35);Config->AngularVelocityScale=.2f;
 // Rebuild the deformed skirt's tangent frame without changing its original material.
 auto& LOD=Mesh->GetImportedModel()->LODModels[0];for(auto& Section:LOD.Sections){int32 M=Section.MaterialIndex;if(Mesh->GetMaterials().IsValidIndex(M)&&Mesh->GetMaterials()[M].MaterialSlotName.ToString().Contains(TEXT("teef_diff_000_a_uni"))){Section.bRecomputeTangent=true;Section.RecomputeTangentsVertexMaskChannel=ESkinVertexColorChannel::None;auto& Source=LOD.UserSectionsData.FindOrAdd(Section.OriginalDataSectionIndex);Source.bRecomputeTangent=true;Source.RecomputeTangentsVertexMaskChannel=ESkinVertexColorChannel::None;}}
 if(auto* Info=Mesh->GetLODInfo(0))Info->SkinCacheUsage=ESkinCacheUsage::Enabled;
 Cloth->InvalidateAllCachedData();return HcxRepairSavedClothBinding(Mesh);
}

FString UHcxRetargetTools::CreateSkirtCloth(USkeletalMesh* Mesh){
 if(!Mesh||!Mesh->GetImportedModel()||!Mesh->GetImportedModel()->LODModels.Num())return TEXT("cloth mesh/LOD missing");
 if(Mesh->GetMeshClothingAssets().Num()){
  auto* Old=Cast<UClothingAssetCommon>(Mesh->GetMeshClothingAssets()[0]);int32 RenderTriangles=0;for(const auto& S:Mesh->GetImportedModel()->LODModels[0].Sections)if(Mesh->GetMaterials().IsValidIndex(S.MaterialIndex)&&Mesh->GetMaterials()[S.MaterialIndex].MaterialSlotName.ToString().Contains(TEXT("teef_diff_000_a_uni")))RenderTriangles+=S.NumTriangles;
  if(!Old||!Old->LodData.Num()||Old->LodData[0].PhysicalMeshData.Indices.Num()/3<=RenderTriangles*1.2f)return HcxRepairSavedClothBinding(Mesh);
  // Recreate the simulation sheet after removing the inward render layer.
  {FScopedSkeletalMeshPostEditChange Scoped(Mesh);Old->UnbindFromSkeletalMesh(Mesh);Mesh->GetMeshClothingAssets().Reset();}FSkinnedAssetCompilingManager::Get().FinishCompilation({Mesh});
 }
 const auto& Ref=Mesh->GetRefSkeleton();TArray<FTransform> Global=Ref.GetRefBonePose();for(int32 I=0;I<Global.Num();++I){int32 P=Ref.GetParentIndex(I);if(P>=0)Global[I]*=Global[P];}
 const TCHAR* PAPath=TEXT("/Game/Hcx2B/PA_2B_Cloth37");auto* PA=LoadObject<UPhysicsAsset>(nullptr,PAPath);
 if(!PA){PA=NewObject<UPhysicsAsset>(CreatePackage(PAPath),TEXT("PA_2B_Cloth37"),RF_Public|RF_Standalone|RF_Transactional);
  int32 Pelvis=Ref.FindBoneIndex(TEXT("SKEL_Pelvis"));if(Pelvis<0)return TEXT("cloth pelvis missing");
  auto* Hips=NewObject<USkeletalBodySetup>(PA);Hips->BoneName=TEXT("SKEL_Pelvis");Hips->PhysicsType=PhysType_Kinematic;
  int32 Back=Ref.FindBoneIndex(TEXT("SM_M_BackSkirtRoll")),Front=Ref.FindBoneIndex(TEXT("SM_M_FrontSkirtRoll"));if(Back<0||Front<0)return TEXT("cloth direction references missing");FVector Rear=Global[Back].GetTranslation()-Global[Front].GetTranslation();Rear.Z=0;Rear.Normalize();
  for(const TCHAR* Side:{TEXT("L"),TEXT("R")}){FName Thigh(*(FString(TEXT("SKEL_"))+Side+TEXT("_Thigh"))),Calf(*(FString(TEXT("SKEL_"))+Side+TEXT("_Calf")));int32 TI=Ref.FindBoneIndex(Thigh),CI=Ref.FindBoneIndex(Calf);if(TI<0||CI<0)return TEXT("cloth thigh missing");
   // Cloth collision only: these bodies are not assigned to the gameplay mesh.
   FKSphereElem Butt;Butt.Center=Global[Pelvis].InverseTransformPosition(Global[TI].GetTranslation()+Rear*4+FVector(0,0,2));Butt.Radius=8;Hips->AggGeom.SphereElems.Add(Butt);
   auto* Body=NewObject<USkeletalBodySetup>(PA);Body->BoneName=Thigh;Body->PhysicsType=PhysType_Kinematic;FVector End=Ref.GetRefBonePose()[CI].GetTranslation();FKSphylElem Capsule;Capsule.Center=End*.5;Capsule.Radius=7;Capsule.Length=FMath::Max(0.f,float(End.Length())-14);Capsule.Rotation=FQuat::FindBetweenNormals(FVector::UpVector,End.GetSafeNormal()).Rotator();Body->AggGeom.SphylElems.Add(Capsule);PA->SkeletalBodySetups.Add(Body);
  }
  PA->SkeletalBodySetups.Add(Hips);PA->UpdateBodySetupIndexMap();PA->UpdateBoundsBodiesArray();PA->MarkPackageDirty();FAssetRegistryModule::AssetCreated(PA);
 }
 TArray<int32> Sections;const auto& LOD=Mesh->GetImportedModel()->LODModels[0];for(int32 I=0;I<LOD.Sections.Num();++I){int32 M=LOD.Sections[I].MaterialIndex;if(Mesh->GetMaterials().IsValidIndex(M)&&Mesh->GetMaterials()[M].MaterialSlotName.ToString().Contains(TEXT("teef_diff_000_a_uni")))Sections.Add(I);}
 if(Sections.Num()!=1)return FString::Printf(TEXT("expected one skirt material section, found %d"),Sections.Num());
 auto* Factory=NewObject<UClothingAssetFactory>();int32 Free=0,Fixed=0;
 for(int32 Section:Sections){FSkeletalMeshClothBuildParams Params;Params.AssetName=TEXT("CA_2B_Skirt37");Params.LodIndex=0;Params.SourceSection=Section;Params.bRemoveFromMesh=false;Params.PhysicsAsset=PA;
  auto* Cloth=Cast<UClothingAssetCommon>(Factory->CreateFromSkeletalMesh(Mesh,Params));if(!Cloth)return TEXT("cloth factory failed");
  auto& L=Cloth->LodData[0];auto& Phys=L.PhysicalMeshData;const int32 Count=Phys.Vertices.Num();if(Count<100)return TEXT("cloth physical mesh unexpectedly empty");
  // Every disconnected panel needs a fixed upper edge, including short side panels.
  TArray<int32> Parent;Parent.SetNumUninitialized(Count);for(int32 I=0;I<Count;++I)Parent[I]=I;auto Root=[&](int32 I){while(Parent[I]!=I){Parent[I]=Parent[Parent[I]];I=Parent[I];}return I;};
  for(int32 I=0;I<Phys.Indices.Num();I+=3){int32 A=Root(Phys.Indices[I]);for(int32 J=1;J<3;++J)Parent[Root(Phys.Indices[I+J])]=A;}
  TMap<int32,float> Tops;for(int32 I=0;I<Count;++I){int32 C=Root(I);Tops.FindOrAdd(C,-BIG_NUMBER)=FMath::Max(Tops.FindOrAdd(C,-BIG_NUMBER),Phys.Vertices[I].Z);}
  FPointWeightMap Max(Count);Max.Name=TEXT("WaistPinned_HemFree");Max.CurrentTarget=(uint8)EWeightMapTargetCommon::MaxDistance;Max.bEnabled=true;
  for(int32 I=0;I<Count;++I){float Anchor=FMath::Min(111.f,Tops[Root(I)]-1.5f);float T=FMath::Clamp((Anchor-Phys.Vertices[I].Z)/28.f,0.f,1.f);Max.Values[I]=35*T*T*(3-2*T);if(Max.Values[I]<.05f){Max.Values[I]=0;++Fixed;}else ++Free;}
  L.PointWeightMaps.Reset();L.PointWeightMaps.Add(Max);L.bSmoothTransition=true;
  auto* Config=NewObject<UChaosClothConfig>(Cloth);Config->MassMode=EClothMassMode::Density;Config->Density=.35f;Config->EdgeStiffnessWeighted.Low=Config->EdgeStiffnessWeighted.High=.95f;Config->BendingStiffnessWeighted.Low=Config->BendingStiffnessWeighted.High=.5f;Config->AreaStiffnessWeighted.Low=Config->AreaStiffnessWeighted.High=.9f;Config->DampingCoefficient=.25f;Config->LocalDampingCoefficient=.15f;Config->CollisionThickness=1.f;Config->FrictionCoefficient=.3f;Config->bUseCCD=true;Config->bUseSelfCollisions=false;Config->bUseGeodesicDistance=false;Config->AnimDriveStiffness.Low=Config->AnimDriveStiffness.High=.35f;Config->AnimDriveDamping.Low=Config->AnimDriveDamping.High=.5f;Config->LinearVelocityScale=FVector(.6);Config->AngularVelocityScale=.4f;Config->GravityScale=.6f;
  auto* Shared=NewObject<UChaosClothSharedSimConfig>(Cloth);Shared->IterationCount=4;Shared->MaxIterationCount=8;Shared->SubdivisionCount=2;Cloth->ClothConfigs.Add(Config->GetClass()->GetFName(),Config);Cloth->ClothConfigs.Add(Shared->GetClass()->GetFName(),Shared);
  Cloth->ApplyParameterMasks(true);Cloth->RefreshBoneMapping(Mesh);Mesh->AddClothingAsset(Cloth);if(!Cloth->BindToSkeletalMesh(Mesh,0,Section,0))return TEXT("cloth section binding failed");
 }
 if(Free<100||Fixed<20)return TEXT("cloth anchors or moving vertices missing");Mesh->PostEditChange();Mesh->MarkPackageDirty();return HcxRepairSavedClothBinding(Mesh);
}
