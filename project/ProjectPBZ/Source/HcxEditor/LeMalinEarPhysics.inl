#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
// Separate from braid physics so ear recovery never changes accepted hair tuning.
static UPhysicsAsset* BuildLeMalinEarPhysics(USkeletalMesh* Mesh)
{
 const TCHAR* Path=TEXT("/Game/LeMalin/PA_LeMalin_Ears");
 const auto& Ref=Mesh->GetRefSkeleton();auto Global=Ref.GetRefBonePose();
 for(int32 I=0;I<Global.Num();++I){int32 P=Ref.GetParentIndex(I);if(P>=0)Global[I]*=Global[P];}
 int32 First=Ref.FindBoneIndex(TEXT("aux_024"));if(First<0)return nullptr;
 int32 Anchor=Ref.GetParentIndex(First);if(Anchor<0)return nullptr;
 auto* Asset=NewObject<UPhysicsAsset>(CreatePackage(Path),TEXT("PA_LeMalin_Ears"),RF_Public|RF_Standalone|RF_Transactional);
 Asset->SolverSettings.FixedTimeStep=1.f/120;Asset->SolverSettings.PositionIterations=12;
 Asset->SolverSettings.VelocityIterations=2;Asset->SolverSettings.ProjectionIterations=1;
 Asset->SolverSettings.bUseLinearJointSolver=true;
 Asset->SolverSettings.MaxDepenetrationVelocity=50.f;
 auto* Contact=LoadObject<UPhysicalMaterial>(nullptr,TEXT("/Game/LeMalin/PM_LeMalinHairContact"));
 auto* Head=NewObject<USkeletalBodySetup>(Asset,NAME_None,RF_Transactional);
 Head->BoneName=Ref.GetBoneName(Anchor);Head->PhysicsType=PhysType_Kinematic;
 Head->DefaultInstance.bEnableGravity=false;Head->DefaultInstance.SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
 Head->AggGeom.SphereElems.Add(FKSphereElem(1.f));Head->InvalidatePhysicsData();Head->CreatePhysicsMeshes();Asset->SkeletalBodySetups.Add(Head);
 for(int32 Side=0;Side<2;++Side)for(int32 Segment=0;Segment<3;++Segment){
  int32 B=Ref.FindBoneIndex(FName(*FString::Printf(TEXT("aux_%03d"),24+Segment*2+Side)));if(B<0)return nullptr;
  int32 Parent=Ref.GetParentIndex(B);if(Parent<0)return nullptr;
  FVector Start=Global[B].GetLocation(),End;
  if(Segment<2){int32 Next=Ref.FindBoneIndex(FName(*FString::Printf(TEXT("aux_%03d"),26+Segment*2+Side)));if(Next<0)return nullptr;End=Global[Next].GetLocation();}
  else End=Start+(Start-Global[Parent].GetLocation())*.65f;
  FVector Direction=(End-Start).GetSafeNormal();float Length=FVector::Distance(Start,End);if(Length<.1f)return nullptr;
  auto* Body=NewObject<USkeletalBodySetup>(Asset,NAME_None,RF_Transactional);
  Body->BoneName=Ref.GetBoneName(B);Body->PhysicsType=PhysType_Simulated;
  Body->DefaultInstance.bOverrideMass=true;Body->DefaultInstance.SetMassOverride(Segment==0?.07f:Segment==1?.045f:.025f);
  Body->DefaultInstance.bEnableGravity=true;Body->DefaultInstance.LinearDamping=.35f;Body->DefaultInstance.AngularDamping=1.2f;
  Body->DefaultInstance.SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
  if(Contact)Body->DefaultInstance.SetPhysMaterialOverride(Contact);
  // Fit low-complexity convex shapes to the actual skinned ear surface. A thin
  // centerline capsule misses the flat ear edges where an X overlap first occurs.
  TArray<FVector> Points;
  if(auto* Imported=Mesh->GetImportedModel())if(Imported->LODModels.Num())for(const auto& Section:Imported->LODModels[0].Sections)for(const auto& V:Section.SoftVertices){
   uint32 Total=0,Weight=0;for(int32 K=0;K<UE_ARRAY_COUNT(V.InfluenceBones);++K){Total+=V.InfluenceWeights[K];if(Section.BoneMap.IsValidIndex(V.InfluenceBones[K])&&Section.BoneMap[V.InfluenceBones[K]]==B)Weight+=V.InfluenceWeights[K];}
   if(!Total||Weight*5<Total)continue;FVector P(V.Position);double Along=FVector::DotProduct(P-Start,Direction);if(Along>=-.5&&Along<=Length+.5)Points.Add(P);
  }
  if(Points.Num()>=4){
   FKConvexElem Shape;
   for(int32 X=-1;X<=1;++X)for(int32 Y=-1;Y<=1;++Y)for(int32 Z=-1;Z<=1;++Z){
    if(!X&&!Y&&!Z)continue;FVector Normal=FVector(X,Y,Z).GetSafeNormal();double Best=-DBL_MAX;FVector Support=Start;
    for(const FVector& P:Points){double Dot=FVector::DotProduct(P,Normal);if(Dot>Best){Best=Dot;Support=P;}}
    Shape.VertexData.Add(Global[B].InverseTransformPosition(Support+Normal*.12));
   }
   Shape.UpdateElemBox();Body->AggGeom.ConvexElems.Add(Shape);
  }else{
   FKSphylElem Shape(.7f,FMath::Max(.1f,Length-1.4f));Shape.Center=Global[B].InverseTransformPosition((Start+End)*.5f);
   Shape.Rotation=FQuat::FindBetweenNormals(FVector::UpVector,Global[B].InverseTransformVectorNoScale(Direction)).Rotator();Body->AggGeom.SphylElems.Add(Shape);
  }
  Body->InvalidatePhysicsData();Body->CreatePhysicsMeshes();Asset->SkeletalBodySetups.Add(Body);
  FTransform Joint(FRotationMatrix::MakeFromX(Direction).ToQuat(),Start);
  auto* Template=NewObject<UPhysicsConstraintTemplate>(Asset,NAME_None,RF_Transactional);auto& C=Template->DefaultInstance;
  C.JointName=Body->BoneName;C.ConstraintBone1=Body->BoneName;C.ConstraintBone2=Ref.GetBoneName(Parent);
  C.SetRefFrame(EConstraintFrame::Frame1,Joint.GetRelativeTransform(Global[B]));
  C.SetRefFrame(EConstraintFrame::Frame2,Joint.GetRelativeTransform(Global[Parent]));
  C.SetLinearLimits(LCM_Locked,LCM_Locked,LCM_Locked,0);C.SetAngularTwistLimit(ACM_Limited,4);
  C.SetAngularSwing1Limit(ACM_Limited,Segment==0?24:Segment==1?28:35);
  C.SetAngularSwing2Limit(ACM_Limited,Segment==0?24:Segment==1?28:35);
  C.SetDisableCollision(true);C.SetProjectionParams(true,1.f,.15f,.5f,15.f);
  C.SetAngularDriveMode(EAngularDriveMode::SLERP);C.SetOrientationDriveSLERP(true);C.SetAngularVelocityDriveSLERP(true);
  C.SetAngularDriveAccelerationMode(true);
  // Upright ears need substantially more recovery than hanging braids. Limits
  // remain symmetric: no built-in forward bend or one-sided directional drive.
  C.SetAngularDriveParams(Segment==0?800.f:Segment==1?600.f:450.f,Segment==0?18.f:Segment==1?14.f:11.f,0.f);
  C.SetAngularOrientationTarget(FQuat::Identity);Template->SetDefaultProfile(C);Asset->ConstraintSetup.Add(Template);
 }
 for(int32 I=0;I<Asset->SkeletalBodySetups.Num();++I)for(int32 J=I+1;J<Asset->SkeletalBodySetups.Num();++J){
  if(I>0&&J>0&&((I<=3)!=(J<=3)))Asset->EnableCollision(I,J);else Asset->DisableCollision(I,J);
 }
 Asset->UpdateBodySetupIndexMap();Asset->UpdateBoundsBodiesArray();Asset->MarkPackageDirty();FAssetRegistryModule::AssetCreated(Asset);
 auto* Upright=Cast<UPhysicsAsset>(StaticDuplicateObject(Asset,CreatePackage(TEXT("/Game/LeMalin/PA_LeMalin_Ears_Upright")),TEXT("PA_LeMalin_Ears_Upright")));if(!Upright)return nullptr;
 for(int32 I=0;I<Upright->ConstraintSetup.Num();++I){int32 Segment=I%3;UPhysicsConstraintTemplate* Template=Upright->ConstraintSetup[I];auto& C=Template->DefaultInstance;
  C.SetAngularSwing1Limit(ACM_Limited,Segment==0?10:Segment==1?16:22);C.SetAngularSwing2Limit(ACM_Limited,Segment==0?10:Segment==1?16:22);
  C.SetAngularDriveParams(Segment==0?4800.f:Segment==1?3000.f:1800.f,Segment==0?60.f:Segment==1?40.f:28.f,0.f);Template->SetDefaultProfile(C);
 }
 Upright->MarkPackageDirty();FAssetRegistryModule::AssetCreated(Upright);return Asset;
}
