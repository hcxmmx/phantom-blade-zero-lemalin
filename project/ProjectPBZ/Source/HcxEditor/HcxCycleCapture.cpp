#include "HcxRetargetTools.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Editor.h"
#include "UObject/UnrealType.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"

bool UHcxRetargetTools::CaptureGaitCycle(USkeletalMesh* SourceMesh,USkeletalMesh* TargetMesh,UAnimSequence* Sequence,UAnimBlueprint* Baseline,UAnimBlueprint* Current,const FString& OutputFile){
 if(!GEditor||!SourceMesh||!TargetMesh||!Sequence||!Baseline||!Current)return false;
 UWorld* World=GEditor->GetEditorWorldContext().World();FActorSpawnParameters Params;Params.ObjectFlags=RF_Transient;auto* Pawn=World->SpawnActor<ACharacter>(Params);if(!Pawn)return false;Pawn->SetActorEnableCollision(false);Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);Pawn->GetCharacterMovement()->Velocity=FVector(300,0,0);
 auto* Source=Pawn->GetMesh();Source->SetSkeletalMesh(SourceMesh);Source->SetDisablePostProcessBlueprint(true);Source->SetAnimationMode(EAnimationMode::AnimationSingleNode);Source->SetAnimation(Sequence);Source->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 SourceMesh->GetSkeleton()->AddCurveMetaData(TEXT("movedata_speed"));auto* Single=Source->GetSingleNodeInstance();Single->SetPlaying(false);Single->SetPreviewCurveOverride(TEXT("movedata_speed"),350.f,false);
 auto CreateTarget=[&](UAnimBlueprint* BP){auto* M=NewObject<USkeletalMeshComponent>(Pawn,NAME_None,RF_Transient);Pawn->AddInstanceComponent(M);M->SetupAttachment(Source);M->RegisterComponent();M->SetSkeletalMesh(TargetMesh);M->SetDisablePostProcessBlueprint(true);M->SetAnimationMode(EAnimationMode::AnimationBlueprint);M->SetAnimInstanceClass(BP->GeneratedClass);M->InitAnim(true);M->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;return M;};
 auto* Base=CreateTarget(Baseline);auto* Live=CreateTarget(Current);
 auto Pose=[](const TArray<FTransform>& Values){TArray<TSharedPtr<FJsonValue>> Out;for(const auto& T:Values){const auto P=T.GetLocation(),S=T.GetScale3D();const auto Q=T.GetRotation();TArray<TSharedPtr<FJsonValue>> V;for(double D:{P.X,P.Y,P.Z,Q.X,Q.Y,Q.Z,Q.W,S.X,S.Y,S.Z})V.Add(MakeShared<FJsonValueNumber>(D));Out.Add(MakeShared<FJsonValueArray>(V));}return Out;};
 auto Skeleton=[&](USkeletalMesh* Mesh){auto J=MakeShared<FJsonObject>();const auto& Ref=Mesh->GetRefSkeleton();TArray<TSharedPtr<FJsonValue>> Names,Parents;auto Global=Ref.GetRefBonePose();for(int32 I=0;I<Ref.GetNum();++I){Names.Add(MakeShared<FJsonValueString>(Ref.GetBoneName(I).ToString()));const int32 Parent=Ref.GetParentIndex(I);Parents.Add(MakeShared<FJsonValueNumber>(Parent));if(Parent>=0)Global[I]=Global[I]*Global[Parent];}J->SetArrayField(TEXT("bones"),Names);J->SetArrayField(TEXT("parents"),Parents);J->SetArrayField(TEXT("reference_global"),Pose(Global));return J;};
 const float DT=1.f/60.f;const float Duration=Sequence->GetPlayLength();const int32 Count=FMath::RoundToInt(Duration*60)+1;
 auto Tick=[&](float Time){Single->SetPosition(Time,false);Source->TickAnimation(0.f,false);Source->RefreshBoneTransforms();for(auto* M:{Base,Live}){M->TickAnimation(DT,false);M->RefreshBoneTransforms();}};
 for(int32 I=0;I<40;++I)Tick(0.f);
 TArray<TSharedPtr<FJsonValue>> SFrames,BFrames,LFrames,States;
 for(int32 I=0;I<Count;++I){Tick(FMath::Min(I*DT,Duration));SFrames.Add(MakeShared<FJsonValueArray>(Pose(Source->GetComponentSpaceTransforms())));BFrames.Add(MakeShared<FJsonValueArray>(Pose(Base->GetComponentSpaceTransforms())));LFrames.Add(MakeShared<FJsonValueArray>(Pose(Live->GetComponentSpaceTransforms())));auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("time"),FMath::Min(I*DT,Duration));J->SetNumberField(TEXT("source_move_curve"),Single->GetCurveValue(TEXT("movedata_speed")));if(auto* Anim=Live->GetAnimInstance())for(const TCHAR* Name:{TEXT("HcxHipWeight"),TEXT("HcxHipAngleL"),TEXT("HcxHipAngleR")})if(auto* Property=FindFProperty<FFloatProperty>(Anim->GetClass(),Name))J->SetNumberField(Name,Property->GetPropertyValue_InContainer(Anim));States.Add(MakeShared<FJsonValueObject>(J));}
 auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),Sequence->GetName());J->SetStringField(TEXT("baseline_blueprint"),Baseline->GetPathName());J->SetStringField(TEXT("current_blueprint"),Current->GetPathName());J->SetNumberField(TEXT("fps"),60);J->SetNumberField(TEXT("duration"),Duration);J->SetStringField(TEXT("limitation"),TEXT("Editor instance of extracted animation, not the game locomotion/ground-IK blueprint. Source/root pose fixed to extracted clip; both targets sampled after same source frame."));
 for(auto Pair:{TPair<const TCHAR*,TArray<TSharedPtr<FJsonValue>>*>(TEXT("source"),&SFrames),TPair<const TCHAR*,TArray<TSharedPtr<FJsonValue>>*>(TEXT("baseline"),&BFrames),TPair<const TCHAR*,TArray<TSharedPtr<FJsonValue>>*>(TEXT("current"),&LFrames)}){auto D=Skeleton(Pair.Key==FString(TEXT("source"))?SourceMesh:TargetMesh);D->SetArrayField(TEXT("frames"),*Pair.Value);J->SetObjectField(Pair.Key,D);}
 J->SetArrayField(TEXT("states"),States);FString Json;FJsonSerializer::Serialize(J,TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json));Pawn->Destroy();return FFileHelper::SaveStringToFile(Json,*OutputFile,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
