// Editor-only authoring of an engine-native, self-contained braid physics asset.
// Runtime uses FAnimNode_RigidBody; no project C++ class is shipped to the game.
static UPhysicsAsset* BuildLeMalinBraidPhysics(USkeletalMesh* Mesh)
{
 const TCHAR* Path=TEXT("/Game/LeMalin/PA_LeMalin_Braids");
 if(auto* Existing=LoadObject<UPhysicsAsset>(nullptr,*(FString(Path)+TEXT(".PA_LeMalin_Braids"))))return Existing;
 FString Text;TSharedPtr<FJsonObject> Audit;
 if(!FFileHelper::LoadFileToString(Text,*FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../../output/hair-investigation/pmx-hair-audit.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Audit))return nullptr;
 const auto& Ref=Mesh->GetRefSkeleton();auto Global=Ref.GetRefBonePose();
 for(int32 I=0;I<Global.Num();++I){int32 P=Ref.GetParentIndex(I);if(P>=0)Global[I]*=Global[P];}
 auto* Asset=NewObject<UPhysicsAsset>(CreatePackage(Path),TEXT("PA_LeMalin_Braids"),RF_Public|RF_Standalone|RF_Transactional);
 Asset->SolverSettings.FixedTimeStep=1.f/120;
 Asset->SolverSettings.PositionIterations=12;Asset->SolverSettings.VelocityIterations=2;Asset->SolverSettings.ProjectionIterations=1;
 Asset->SolverSettings.bUseLinearJointSolver=true;
 Asset->SolverSettings.MaxDepenetrationVelocity=100.f;
 auto* Contact=NewObject<UPhysicalMaterial>(CreatePackage(TEXT("/Game/LeMalin/PM_LeMalinHairContact")),TEXT("PM_LeMalinHairContact"),RF_Public|RF_Standalone);
 Contact->Friction=.05f;Contact->StaticFriction=.05f;Contact->Restitution=0;
 Contact->bOverrideFrictionCombineMode=true;Contact->FrictionCombineMode=EFrictionCombineMode::Min;
 Contact->bOverrideRestitutionCombineMode=true;Contact->RestitutionCombineMode=EFrictionCombineMode::Min;
 Contact->MarkPackageDirty();FAssetRegistryModule::AssetCreated(Contact);
 for(int32 Side=0;Side<2;++Side)for(int32 Segment=0;Segment<=6;++Segment){
  FName Bone(*FString::Printf(TEXT("aux_%03d"),Segment*2+Side));int32 B=Ref.FindBoneIndex(Bone);if(B<0)return nullptr;
  auto* Body=NewObject<USkeletalBodySetup>(Asset,NAME_None,RF_Transactional);Body->BoneName=Bone;Body->PhysicsType=Segment==0?PhysType_Kinematic:PhysType_Simulated;
  Body->DefaultInstance.bOverrideMass=true;Body->DefaultInstance.SetMassOverride(.5f);Body->DefaultInstance.bEnableGravity=true;
  Body->DefaultInstance.LinearDamping=.35f;Body->DefaultInstance.AngularDamping=1.6f;
  Body->DefaultInstance.SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
  Body->DefaultInstance.SetPhysMaterialOverride(Contact);
  if(Segment==0){FKSphereElem Sphere(1.f);Body->AggGeom.SphereElems.Add(Sphere);}
  else {
   FString Name=FString::Printf(TEXT("Ex_BackHairA%02d_%s"),Segment,Side?TEXT("R"):TEXT("L"));TSharedPtr<FJsonObject> Raw;
   for(const auto& V:Audit->GetArrayField(TEXT("rigids")))if(V->AsObject()->GetStringField(TEXT("bound_bone"))==Name){Raw=V->AsObject();break;}
   if(!Raw)return nullptr;
   const auto& P=Raw->GetArrayField(TEXT("location"));const auto& Size=Raw->GetArrayField(TEXT("size"));
   // PMX coordinates -> imported UE component coordinates. Align the diagnostic
   // PMX height to this mesh, rather than assuming its floor offset is zero.
   double HeightOffset=0;for(const auto& V:Audit->GetArrayField(TEXT("bones")))if(V->AsObject()->GetStringField(TEXT("name"))==Name){HeightOffset=Global[B].GetLocation().Z-V->AsObject()->GetArrayField(TEXT("position_cm"))[2]->AsNumber();break;}
   FVector Center(P[0]->AsNumber()*8,-P[2]->AsNumber()*8,P[1]->AsNumber()*8+HeightOffset);
   int32 Next=Ref.FindBoneIndex(FName(*FString::Printf(TEXT("aux_%03d"),(Segment+1)*2+Side)));if(Next<0)return nullptr;
   FVector Direction=(Global[Next].GetLocation()-Global[B].GetLocation()).GetSafeNormal();
   FKSphylElem Shape(Size[0]->AsNumber()*8,Size[1]->AsNumber()*8);
   Shape.Center=Global[B].InverseTransformPosition(Center);
   Shape.Rotation=FQuat::FindBetweenNormals(FVector::UpVector,Global[B].InverseTransformVectorNoScale(Direction)).Rotator();
   Body->AggGeom.SphylElems.Add(Shape);Body->DefaultInstance.SetMassOverride(Raw->GetNumberField(TEXT("mass")));
  }
  Body->InvalidatePhysicsData();Body->CreatePhysicsMeshes();Asset->SkeletalBodySetups.Add(Body);
  if(Segment>=1){
   int32 Parent=Ref.FindBoneIndex(FName(*FString::Printf(TEXT("aux_%03d"),(Segment-1)*2+Side)));
   int32 Next=Ref.FindBoneIndex(FName(*FString::Printf(TEXT("aux_%03d"),(Segment+1)*2+Side)));
   FVector Axis=(Global[Next].GetLocation()-Global[B].GetLocation()).GetSafeNormal();
   FTransform Joint(FRotationMatrix::MakeFromX(Axis).ToQuat(),Global[B].GetLocation());
   auto* Template=NewObject<UPhysicsConstraintTemplate>(Asset,NAME_None,RF_Transactional);auto& C=Template->DefaultInstance;
   C.JointName=Bone;C.ConstraintBone1=Bone;C.ConstraintBone2=Ref.GetBoneName(Parent);
   C.SetRefFrame(EConstraintFrame::Frame1,Joint.GetRelativeTransform(Global[B]));
   C.SetRefFrame(EConstraintFrame::Frame2,Joint.GetRelativeTransform(Global[Parent]));
   C.SetLinearLimits(LCM_Locked,LCM_Locked,LCM_Locked,0);
   C.SetAngularTwistLimit(ACM_Limited,8);
   C.SetAngularSwing1Limit(ACM_Limited,Segment==1?30:Segment==2?40:55);
   C.SetAngularSwing2Limit(ACM_Limited,Segment==1?30:Segment==2?40:55);
   C.SetDisableCollision(true);C.SetProjectionParams(true,1.f,.15f,.5f,15.f);
   C.SetAngularDriveMode(EAngularDriveMode::SLERP);C.SetOrientationDriveSLERP(true);C.SetAngularVelocityDriveSLERP(true);
   C.SetAngularDriveAccelerationMode(true);C.SetAngularDriveParams(10.f,2.f,0.f);
   C.SetAngularOrientationTarget(FQuat::Identity);
   // Constraint templates serialize DefaultProfile rather than the currently
   // edited instance. Commit it explicitly so limits/drives survive save/cook.
   Template->SetDefaultProfile(C);
   Asset->ConstraintSetup.Add(Template);
  }
 }
 const int32 HairCount=Asset->SkeletalBodySetups.Num();
 FString BodyText;TArray<TSharedPtr<FJsonValue>> Shapes;
 if(!FFileHelper::LoadFileToString(BodyText,*FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../../metadata/body-colliders.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(BodyText),Shapes))return nullptr;
 TMap<FName,USkeletalBodySetup*> BodyByBone;
 for(const auto& V:Shapes){
  auto S=V->AsObject();FName Bone(*S->GetStringField(TEXT("bone")));int32 B=Ref.FindBoneIndex(Bone);if(B<0)return nullptr;
  USkeletalBodySetup* Body=BodyByBone.FindRef(Bone);
  if(!Body){Body=NewObject<USkeletalBodySetup>(Asset,NAME_None,RF_Transactional);Body->BoneName=Bone;Body->PhysicsType=PhysType_Kinematic;Body->DefaultInstance.bEnableGravity=false;Body->DefaultInstance.SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);Body->DefaultInstance.SetPhysMaterialOverride(Contact);BodyByBone.Add(Bone,Body);Asset->SkeletalBodySetups.Add(Body);}
  const auto& P=S->GetArrayField(TEXT("center_cm"));const auto& A=S->GetArrayField(TEXT("axis"));
  FVector Center(P[0]->AsNumber(),P[1]->AsNumber(),P[2]->AsNumber());FVector Axis(A[0]->AsNumber(),A[1]->AsNumber(),A[2]->AsNumber());
  if(S->GetStringField(TEXT("shape"))==TEXT("sphere")){FKSphereElem E(S->GetNumberField(TEXT("radius_cm")));E.Center=Global[B].InverseTransformPosition(Center);Body->AggGeom.SphereElems.Add(E);}
  else{FKSphylElem E(S->GetNumberField(TEXT("radius_cm")),S->GetNumberField(TEXT("length_cm")));E.Center=Global[B].InverseTransformPosition(Center);E.Rotation=FQuat::FindBetweenNormals(FVector::UpVector,Global[B].InverseTransformVectorNoScale(Axis).GetSafeNormal()).Rotator();Body->AggGeom.SphylElems.Add(E);}
 }
 for(const auto& Pair:BodyByBone){Pair.Value->InvalidatePhysicsData();Pair.Value->CreatePhysicsMeshes();}
 // Only dynamic braids vs. kinematic anatomy. Anchors, braid self-collision and
 // body/body contacts remain disabled. Body bones are never simulated.
 for(int32 I=0;I<Asset->SkeletalBodySetups.Num();++I)for(int32 J=I+1;J<Asset->SkeletalBodySetups.Num();++J){
  const bool HairBody=I<HairCount&&J>=HairCount&&Asset->SkeletalBodySetups[I]->PhysicsType==PhysType_Simulated;
  if(!HairBody)Asset->DisableCollision(I,J);
 }
 Asset->UpdateBodySetupIndexMap();Asset->UpdateBoundsBodiesArray();Asset->MarkPackageDirty();FAssetRegistryModule::AssetCreated(Asset);
 return Asset;
}
