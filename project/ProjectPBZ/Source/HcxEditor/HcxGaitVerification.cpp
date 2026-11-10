#include "HcxRetargetTools.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Editor.h"
#include "UObject/UnrealType.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

FString UHcxRetargetTools::CheckGaitGate(UAnimBlueprint* Blueprint,USkeletalMesh* SourceMesh,USkeletalMesh* TargetMesh,UAnimSequence* Sequence) {
 if(!Blueprint||!SourceMesh||!TargetMesh||!Sequence||!GEditor)return TEXT("{}");
 UWorld* World=GEditor->GetEditorWorldContext().World();FActorSpawnParameters Params;Params.ObjectFlags=RF_Transient;
 ACharacter* Pawn=World->SpawnActor<ACharacter>(Params);if(!Pawn)return TEXT("{}");Pawn->SetActorEnableCollision(false);
 auto* Source=Pawn->GetMesh();Source->SetSkeletalMesh(SourceMesh);Source->SetAnimationMode(EAnimationMode::AnimationSingleNode);Source->SetAnimation(Sequence);
 SourceMesh->GetSkeleton()->AddCurveMetaData(TEXT("movedata_speed"));
 auto* Single=Source->GetSingleNodeInstance();Single->SetPreviewCurveOverride(TEXT("movedata_speed"),350.f,false);Single->SetPosition(Sequence->GetPlayLength()*.2f,false);
 Source->TickAnimation(1.f/30.f,false);Source->RefreshBoneTransforms();
 auto* Target=NewObject<USkeletalMeshComponent>(Pawn,NAME_None,RF_Transient);Pawn->AddInstanceComponent(Target);Target->SetupAttachment(Source);Target->RegisterComponent();Target->SetSkeletalMesh(TargetMesh);Target->SetDisablePostProcessBlueprint(true);Target->SetAnimationMode(EAnimationMode::AnimationBlueprint);Target->SetAnimInstanceClass(Blueprint->GeneratedClass);Target->InitAnim(true);
 UAnimInstance* Anim=Target->GetAnimInstance();if(!Anim){Pawn->Destroy();return TEXT("{}");}
 auto Read=[&](const TCHAR* Name){auto* Property=FindFProperty<FFloatProperty>(Anim->GetClass(),Name);return Property?Property->GetPropertyValue_InContainer(Anim):-1.f;};
 auto Values=[&](const TCHAR* Label){for(int32 I=0;I<20;++I)Anim->BlueprintUpdateAnimation(1.f/30.f);auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("state"),Label);Row->SetNumberField(TEXT("left_alpha"),Read(TEXT("HcxGaitAlphaL")));Row->SetNumberField(TEXT("right_alpha"),Read(TEXT("HcxGaitAlphaR")));Row->SetNumberField(TEXT("left_shoe_roll"),Read(TEXT("HcxShoeRollL")));Row->SetNumberField(TEXT("right_shoe_roll"),Read(TEXT("HcxShoeRollR")));Row->SetNumberField(TEXT("hinge_weight"),Read(TEXT("HcxHingeWeight")));Row->SetNumberField(TEXT("left_knee_angle"),Read(TEXT("HcxKneeAngleL")));Row->SetNumberField(TEXT("right_knee_angle"),Read(TEXT("HcxKneeAngleR")));Row->SetNumberField(TEXT("hip_weight"),Read(TEXT("HcxHipWeight")));Row->SetNumberField(TEXT("left_hip_correction"),Read(TEXT("HcxHipAngleL")));Row->SetNumberField(TEXT("right_hip_correction"),Read(TEXT("HcxHipAngleR")));return Row;};
 auto Result=MakeShared<FJsonObject>();Result->SetNumberField(TEXT("source_move_curve"),Single->GetCurveValue(TEXT("movedata_speed")));TArray<TSharedPtr<FJsonValue>> Cases;
 auto WidthCase=[&](const TCHAR* Label){auto Row=Values(Label);Row->SetNumberField(TEXT("width_weight"),Read(TEXT("HcxWidthWeight")));return Row;};
 auto* Movement=Pawn->GetCharacterMovement();Movement->SetMovementMode(MOVE_Walking);Movement->Velocity=FVector(300,0,0);Cases.Add(MakeShared<FJsonValueObject>(WidthCase(TEXT("moving_ground"))));
 // Exercise the final animation graph, rather than checking only its input variables.
 const TCHAR* BaselineClass=Blueprint->GetName().Contains(TEXT("Hip"))?TEXT("/Game/Hcx2B/ABP_2B_Adult.ABP_2B_Adult_C"):TEXT("/Game/Hcx2B/ABP_2B_Gait.ABP_2B_Gait_C");
 auto* Baseline=NewObject<USkeletalMeshComponent>(Pawn,NAME_None,RF_Transient);Pawn->AddInstanceComponent(Baseline);Baseline->SetupAttachment(Source);Baseline->RegisterComponent();Baseline->SetSkeletalMesh(TargetMesh);Baseline->SetDisablePostProcessBlueprint(true);Baseline->SetAnimationMode(EAnimationMode::AnimationBlueprint);Baseline->SetAnimInstanceClass(LoadObject<UClass>(nullptr,BaselineClass));Baseline->InitAnim(true);
 Target->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;Baseline->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 for(int32 I=0;I<20;++I)if(auto* BaseAnim=Baseline->GetAnimInstance())BaseAnim->BlueprintUpdateAnimation(1.f/30.f);
 for(int32 I=0;I<5;++I){Baseline->TickAnimation(1.f/30.f,false);Baseline->RefreshBoneTransforms();Target->TickAnimation(1.f/30.f,false);Target->RefreshBoneTransforms();}
 TArray<TSharedPtr<FJsonValue>> PoseChanges;
 for(FName Bone:{FName(TEXT("SKEL_L_Foot")),FName(TEXT("SKEL_R_Foot"))}){
  const FTransform A=Baseline->GetSocketTransform(Bone,RTS_Component),B=Target->GetSocketTransform(Bone,RTS_Component);auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("bone"),Bone.ToString());Row->SetNumberField(TEXT("rotation_delta_degrees"),FMath::RadiansToDegrees(A.GetRotation().AngularDistance(B.GetRotation())));Row->SetNumberField(TEXT("position_delta_cm"),FVector::Distance(A.GetTranslation(),B.GetTranslation()));Row->SetNumberField(TEXT("evaluated_roll_input"),Read(Bone==TEXT("SKEL_L_Foot")?TEXT("HcxShoeRollL"):TEXT("HcxShoeRollR")));PoseChanges.Add(MakeShared<FJsonValueObject>(Row));
 }
 Result->SetArrayField(TEXT("final_graph_foot_changes"),PoseChanges);
 TArray<TSharedPtr<FJsonValue>> Knees;
 for(bool Left:{true,false}){
  const FName Calf=Left?TEXT("SKEL_L_Calf"):TEXT("SKEL_R_Calf"),Thigh=Left?TEXT("SKEL_L_Thigh"):TEXT("SKEL_R_Thigh");auto Row=MakeShared<FJsonObject>();const FTransform T=Target->GetSocketTransform(Calf,RTS_Component),Parent=Target->GetSocketTransform(Thigh,RTS_Component);const FQuat Q=T.GetRelativeTransform(Parent).GetRotation();TArray<TSharedPtr<FJsonValue>> Quat;
  for(double V:{Q.X,Q.Y,Q.Z,Q.W})Quat.Add(MakeShared<FJsonValueNumber>(V));Row->SetStringField(TEXT("side"),Left?TEXT("L"):TEXT("R"));Row->SetArrayField(TEXT("local_quat"),Quat);Row->SetNumberField(TEXT("source_knee_angle_degrees"),Read(Left?TEXT("HcxKneeAngleL"):TEXT("HcxKneeAngleR")));Row->SetNumberField(TEXT("hinge_weight"),Read(TEXT("HcxHingeWeight")));Row->SetNumberField(TEXT("knee_position_delta_cm"),FVector::Distance(T.GetTranslation(),Baseline->GetSocketTransform(Calf,RTS_Component).GetTranslation()));Knees.Add(MakeShared<FJsonValueObject>(Row));
 }
 Result->SetArrayField(TEXT("final_graph_knees"),Knees);
 TArray<TSharedPtr<FJsonValue>> Hips;
 for(bool Left:{true,false})if(auto* Property=FindFProperty<FStructProperty>(Anim->GetClass(),Left?TEXT("HcxHipTargetL"):TEXT("HcxHipTargetR"))){
  const FName Thigh=Left?TEXT("SKEL_L_Thigh"):TEXT("SKEL_R_Thigh"),Roll=Left?TEXT("RB_L_ThighRoll"):TEXT("RB_R_ThighRoll");const FTransform Hip=Target->GetSocketTransform(Thigh,RTS_Component),Pelvis=Target->GetSocketTransform(TEXT("SKEL_Pelvis"),RTS_Component);const FQuat Local=Hip.GetRelativeTransform(Pelvis).GetRotation(),Wanted=Property->ContainerPtrToValuePtr<FRotator>(Anim)->Quaternion();auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("side"),Left?TEXT("L"):TEXT("R"));Row->SetNumberField(TEXT("correction_degrees"),Read(Left?TEXT("HcxHipAngleL"):TEXT("HcxHipAngleR")));Row->SetNumberField(TEXT("hip_position_delta_cm"),FVector::Distance(Hip.GetTranslation(),Baseline->GetSocketTransform(Thigh,RTS_Component).GetTranslation()));Row->SetNumberField(TEXT("desired_rotation_error_degrees"),FMath::RadiansToDegrees(Local.AngularDistance(Wanted)));
  const FQuat Offset=Hip.GetRotation().Inverse()*Target->GetSocketTransform(Roll,RTS_Component).GetRotation();TArray<TSharedPtr<FJsonValue>> Q;for(double V:{Offset.X,Offset.Y,Offset.Z,Offset.W})Q.Add(MakeShared<FJsonValueNumber>(V));Row->SetArrayField(TEXT("roll_offset_quat"),Q);Hips.Add(MakeShared<FJsonValueObject>(Row));
 }
 Result->SetArrayField(TEXT("final_graph_hips"),Hips);
 Movement->Velocity=FVector::ZeroVector;Cases.Add(MakeShared<FJsonValueObject>(WidthCase(TEXT("stopped"))));
 Movement->Velocity=FVector(300,0,0);Movement->SetMovementMode(MOVE_Falling);Cases.Add(MakeShared<FJsonValueObject>(WidthCase(TEXT("airborne"))));
 Movement->SetMovementMode(MOVE_Walking);
 UAnimInstance* Main=Source->GetAnimInstance();auto* Montage=UAnimMontage::CreateSlotAnimationAsDynamicMontage(Sequence,TEXT("DefaultSlot"),.1f,.1f,1.f,1);float Played=Main?Main->Montage_Play(Montage):0.f;Result->SetNumberField(TEXT("montage_play_length"),Played);Result->SetBoolField(TEXT("source_montage_playing"),Main&&Main->IsAnyMontagePlaying());Cases.Add(MakeShared<FJsonValueObject>(WidthCase(TEXT("montage_playing"))));
 Result->SetArrayField(TEXT("cases"),Cases);Pawn->Destroy();FString Json;FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Json));return Json;
}
