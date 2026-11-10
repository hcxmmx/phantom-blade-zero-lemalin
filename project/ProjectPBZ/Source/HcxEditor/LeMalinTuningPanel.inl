#include "LeMalinPlayerPanel.inl"
// Editor generation only. Shipped widgets, save settings and animation nodes
// depend exclusively on engine runtime classes.
FString UHcxRetargetTools::BuildLeMalinTuningPanel(UAnimBlueprint* BP)
{
 using namespace HcxPanel;
 if(!BP)return TEXT("tuning blueprint missing");
 const bool PlayerRelease=FPlatformMisc::GetEnvironmentVariable(TEXT("LEMALIN_PLAYER_RELEASE"))==TEXT("1");
 struct Control {const TCHAR* Name;const TCHAR* Caption;double Min,Max,Default;const TCHAR* Member;double Factor;};
 Control Controls[]={
  {TEXT("GazeMode"),TEXT("眼神（0 关闭 / 1 自动 / 2 左 / 3 右 / 4 上 / 5 下）"),0,5,1,TEXT("LeMalinGazeMode"),1},
  {TEXT("GazeStrength"),TEXT("眼神幅度 (%)"),0,200,100,TEXT("LeMalinGazeStrength"),.01},
  {TEXT("ExpressionMode"),TEXT("表情（0 原样 / 1 慵懒 / 2 微笑 / 3 认真 / 4 自动）"),0,4,4,TEXT("LeMalinExpressionMode"),1},
  {TEXT("ExpressionStrength"),TEXT("表情力度 (%)"),0,150,100,TEXT("LeMalinExpressionStrength"),.01},
  {TEXT("OutfitStyle"),TEXT("服饰（0 白 / 1 黑 / 2 哑光薄袜 / 3 镜亮 / 4 上版全哑 / 5 平滑 / 6 波纹）"),0,6,0,TEXT("LeMalinOutfitStyle"),1},
  {TEXT("EyeFinish"),TEXT("眼睛效果（0 原样 / 1 柔亮 / 2 水晶 / 3 宝石）"),0,3,1,TEXT("LeMalinEyeFinish"),1},
  {TEXT("ReflectanceLevel"),TEXT("底色档位（推荐 2 / 0 原样 → 4 更低 / 5 纯黑定位）"),0,5,2,TEXT("LeMalinReflectanceLevel"),1},
  {TEXT("BackDistance"),TEXT("背挂后移 (cm)"),0,40,25,TEXT("LeMalinBackDistance"),1},
  {TEXT("BackVisible"),TEXT("背部武器（0 隐藏 / 1 显示）"),0,1,1,TEXT("LeMalinBackVisible"),1},
  {TEXT("EarStyle"),TEXT("兔耳（0 软耳 / 1 竖耳）"),0,1,0,TEXT("LeMalinEarStyle"),1},
  {TEXT("Scale"),TEXT("模型大小 (%)"),125,150,125,TEXT("LeMalinScale"),.01},
  {TEXT("Inertia"),TEXT("整体惯性 (%)"),0,100,55,TEXT("LeMalinInertia"),.01},
  {TEXT("Collision"),TEXT("身体碰撞（0 关闭 / 1 开启）"),0,1,1,TEXT("LeMalinCollision"),1},
  {TEXT("Damping"),TEXT("回摆收敛 (1 轻柔 / 2 标准 / 3 稳重 / 4 强)"),1,4,2,TEXT("LeMalinDamping"),1},
  {TEXT("Gravity"),TEXT("重力 (%)"),50,150,100,TEXT("LeMalinGravity"),.01}
 };
 if(PlayerRelease)for(auto& C:Controls)if(const auto* Setting=FindLeMalinPlayerSetting(C.Name))C.Default=Setting->Default;
 const FString Slot=PlayerRelease?TEXT("Hcxmmx_LeMalin_Player_v1"):TEXT("Hcxmmx_LeMalin_Settings_v1");
 auto* SaveBP=FKismetEditorUtilities::CreateBlueprint(USaveGame::StaticClass(),CreatePackage(TEXT("/Game/LeMalin/SG_LeMalinSettings")),TEXT("SG_LeMalinSettings"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
 auto* Widget=Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(UUserWidget::StaticClass(),CreatePackage(TEXT("/Game/LeMalin/WBP_LeMalinTuning")),TEXT("WBP_LeMalinTuning"),BPTYPE_Normal,UWidgetBlueprint::StaticClass(),UWidgetBlueprintGeneratedClass::StaticClass()));
 if(!SaveBP||!Widget)return TEXT("tuning asset creation failed");
 FEdGraphPinType Number;Number.PinCategory=UEdGraphSchema_K2::PC_Real;Number.PinSubCategory=UEdGraphSchema_K2::PC_Double;
 for(const auto& C:Controls)if(!PlayerRelease||FindLeMalinPlayerSetting(C.Name))FBlueprintEditorUtils::AddMemberVariable(SaveBP,C.Name,Number,FString::SanitizeFloat(C.Default));
 if(!PlayerRelease)FBlueprintEditorUtils::AddMemberVariable(SaveBP,TEXT("Turn"),Number,TEXT("0")); // legacy save compatibility
 FEdGraphPinType String;String.PinCategory=UEdGraphSchema_K2::PC_String;
 if(!PlayerRelease){
  FBlueprintEditorUtils::AddMemberVariable(SaveBP,TEXT("BackDiagnostics"),String);
  FBlueprintEditorUtils::AddMemberVariable(Widget,TEXT("BackDiagnostics"),String);
 }
 for(auto& V:SaveBP->NewVariables)V.PropertyFlags|=CPF_SaveGame;
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(SaveBP);FKismetEditorUtilities::CompileBlueprint(SaveBP);SaveBP->MarkPackageDirty();
 FBlueprintEditorUtils::AddMemberVariable(Widget,TEXT("SettingsSlot"),String,Slot);
 if(PlayerRelease){
  FString Result=BuildLeMalinPlayerWidget(Widget,SaveBP);if(!Result.StartsWith(TEXT("OK")))return Result;
 }else{
 UWidgetTree* Tree=Widget->WidgetTree;auto* Root=Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("Root"));Tree->RootWidget=Root;Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
 auto* Border=Tree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("ControlsPanel"));Border->bIsVariable=true;Border->SetBrushColor(FLinearColor(.025f,.03f,.045f,.96f));Border->SetPadding(FMargin(20));Border->SetVisibility(ESlateVisibility::Collapsed);
 auto* PanelSlot=Root->AddChildToCanvas(Border);PanelSlot->SetPosition(FVector2D(28,85));PanelSlot->SetSize(FVector2D(510,690));
 auto* Layout=Tree->ConstructWidget<UVerticalBox>();Border->SetContent(Layout);auto* Scroll=Tree->ConstructWidget<UScrollBox>();Layout->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));auto* Body=Tree->ConstructWidget<UVerticalBox>();Scroll->AddChild(Body);
 auto Label=[&](FName Name,const FString& Text,int32 Size){auto* L=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);L->bIsVariable=true;L->SetText(FText::FromString(Text));auto Font=L->GetFont();Font.Size=Size;L->SetFont(Font);L->SetAutoWrapText(true);Body->AddChildToVerticalBox(L)->SetPadding(FMargin(0,4));return L;};

 Label(TEXT("Title"),PlayerRelease?TEXT("LeMalin / F8"):TEXT("LeMalin 0.2.0-dev.26 / F8"),22);
 Label(TEXT("Hint"),TEXT("眼神默认自动，左右按角色自身方向。2–5 为方向预览，0 回正。自动视线会停留，移动时回正；表情 4 为自动。"),14);
 for(const auto& C:Controls){
  Label(FName(*(FString(C.Name)+TEXT("Caption"))),C.Caption,15);
  Label(FName(*(FString(C.Name)+TEXT("Value"))),FString::SanitizeFloat(C.Default),16);
  auto* S=Tree->ConstructWidget<USlider>(USlider::StaticClass(),FName(*(FString(C.Name)+TEXT("Slider"))));S->bIsVariable=true;S->IsFocusable=false;S->MouseUsesStep=true;
  if(FString(C.Name)==TEXT("Turn"))S->SetIsEnabled(false);S->SetMinValue(C.Min);S->SetMaxValue(C.Max);S->SetStepSize(1);S->SetValue(C.Default);Body->AddChildToVerticalBox(S)->SetPadding(FMargin(0,0,0,7));
 }
 auto* Buttons=Tree->ConstructWidget<UHorizontalBox>();Layout->AddChildToVerticalBox(Buttons)->SetPadding(FMargin(0,10));
 for(auto Entry:{TPair<FName,FString>(TEXT("ResetButton"),TEXT(" 恢复推荐 ")),TPair<FName,FString>(TEXT("SaveButton"),TEXT(" 保存设置 ")),TPair<FName,FString>(TEXT("CloseButton"),TEXT(" 收起 / F8 "))}){
  auto* B=Tree->ConstructWidget<UButton>(UButton::StaticClass(),Entry.Key);B->bIsVariable=true;FindFProperty<FBoolProperty>(UButton::StaticClass(),TEXT("IsFocusable"))->SetPropertyValue_InContainer(B,false);auto* L=Tree->ConstructWidget<UTextBlock>();L->SetText(FText::FromString(Entry.Value));auto ButtonFont=L->GetFont();ButtonFont.Size=16;L->SetFont(ButtonFont);B->SetContent(L);Buttons->AddChildToHorizontalBox(B)->SetPadding(FMargin(0,0,8,0));
 }
 auto* StatusLabel=Label(TEXT("Status"),TEXT("保存后，下次启动自动沿用。"),14);StatusLabel->RemoveFromParent();Layout->AddChildToVerticalBox(StatusLabel)->SetPadding(FMargin(0,4));
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Widget);FKismetEditorUtilities::CompileBlueprint(Widget);
 Graph W{Widget,Widget->UbergraphPages[0]};
 auto Read=[&](const Control& C){auto* N=W.Call(USlider::StaticClass(),TEXT("GetValue"));W.Link(W.Get(FName(*(FString(C.Name)+TEXT("Slider")))),W.P(N,UEdGraphSchema_K2::PN_Self));return W.Ret(N);};
 auto Clamp=[&](const Control& C,UEdGraphPin* V){if(FString(C.Name)==TEXT("Turn")||FString(C.Name)==TEXT("HDRMode"))return W.Number(0);auto* N=W.Call(UKismetMathLibrary::StaticClass(),TEXT("FClamp"));W.Link(V,W.P(N,TEXT("Value")));W.Lit(N,TEXT("Min"),FString::SanitizeFloat(C.Min));W.Lit(N,TEXT("Max"),FString::SanitizeFloat(C.Max));return W.Ret(N);};
 auto Status=[&](UEdGraphPin* Exec,const FString& Message){auto* T=W.Call(UKismetTextLibrary::StaticClass(),TEXT("Conv_StringToText"));W.Lit(T,TEXT("InString"),Message);auto* N=W.Call(UTextBlock::StaticClass(),TEXT("SetText"));W.Link(W.Get(TEXT("Status")),W.P(N,UEdGraphSchema_K2::PN_Self));W.Link(W.Ret(T),W.P(N,TEXT("InText")));return W.Run(Exec,N);};
 for(const auto& C:Controls){auto* E=W.Bound(FName(*(FString(C.Name)+TEXT("Slider"))),USlider::StaticClass(),TEXT("OnValueChanged"));W.Text(W.P(E,UEdGraphSchema_K2::PN_Then),FName(*(FString(C.Name)+TEXT("Value"))),W.P(E,TEXT("Value")));}
 auto* Reset=W.Bound(TEXT("ResetButton"),UButton::StaticClass(),TEXT("OnClicked"));auto* Exec=W.P(Reset,UEdGraphSchema_K2::PN_Then);
 for(const auto& C:Controls){Exec=W.Slider(Exec,FName(*(FString(C.Name)+TEXT("Slider"))),W.Number(C.Default));Exec=W.Text(Exec,FName(*(FString(C.Name)+TEXT("Value"))),W.Number(C.Default));}Status(Exec,TEXT("已恢复推荐值；点击保存可记住。"));
 auto* Construct=W.Event(UUserWidget::StaticClass(),TEXT("Construct"));auto* Exists=W.Call(UGameplayStatics::StaticClass(),TEXT("DoesSaveGameExist"));W.Link(W.Get(TEXT("SettingsSlot")),W.P(Exists,TEXT("SlotName")));auto* HasSave=W.Branch(W.Run(W.P(Construct,UEdGraphSchema_K2::PN_Then),Exists),W.Ret(Exists));
 auto* Load=W.Call(UGameplayStatics::StaticClass(),TEXT("LoadGameFromSlot"));W.Link(W.Get(TEXT("SettingsSlot")),W.P(Load,TEXT("SlotName")));auto* Loaded=W.Cast(W.Ret(Load),SaveBP->GeneratedClass);auto* ValidSave=W.Branch(W.Run(W.P(HasSave,UEdGraphSchema_K2::PN_Then),Load),W.Valid(Loaded));Exec=W.P(ValidSave,UEdGraphSchema_K2::PN_Then);
 for(const auto& C:Controls){auto* V=Clamp(C,W.Get(C.Name,SaveBP->GeneratedClass,Loaded));Exec=W.Slider(Exec,FName(*(FString(C.Name)+TEXT("Slider"))),V);Exec=W.Text(Exec,FName(*(FString(C.Name)+TEXT("Value"))),V);}Status(Exec,TEXT("已载入上次保存的设置。"));
 auto* SaveEvent=W.Bound(TEXT("SaveButton"),UButton::StaticClass(),TEXT("OnClicked"));auto* CreateSave=W.Call(UGameplayStatics::StaticClass(),TEXT("CreateSaveGameObject"));W.Obj(CreateSave,TEXT("SaveGameClass"),SaveBP->GeneratedClass);Exec=W.Run(W.P(SaveEvent,UEdGraphSchema_K2::PN_Then),CreateSave);auto* SaveObject=W.Cast(W.Ret(CreateSave),SaveBP->GeneratedClass);
 for(const auto& C:Controls){auto* Store=W.Set(C.Name,SaveBP->GeneratedClass,SaveObject);W.Link(Clamp(C,Read(C)),W.P(Store,C.Name));Exec=W.Run(Exec,Store);}
 auto* Diagnostic=W.Set(TEXT("BackDiagnostics"),SaveBP->GeneratedClass,SaveObject);W.Link(W.Get(TEXT("BackDiagnostics")),W.P(Diagnostic,TEXT("BackDiagnostics")));Exec=W.Run(Exec,Diagnostic);
 auto* Save=W.Call(UGameplayStatics::StaticClass(),TEXT("SaveGameToSlot"));W.Link(SaveObject,W.P(Save,TEXT("SaveGameObject")));W.Link(W.Get(TEXT("SettingsSlot")),W.P(Save,TEXT("SlotName")));auto* Saved=W.Branch(W.Run(Exec,Save),W.Ret(Save));Status(W.P(Saved,UEdGraphSchema_K2::PN_Then),TEXT("设置已保存，下次启动自动沿用。"));Status(W.P(Saved,UEdGraphSchema_K2::PN_Else),TEXT("未能保存；本次实时调整仍有效。"));
 auto* Close=W.Bound(TEXT("CloseButton"),UButton::StaticClass(),TEXT("OnClicked"));auto* Hide=W.Call(UWidget::StaticClass(),TEXT("SetVisibility"));W.Link(W.Get(TEXT("ControlsPanel")),W.P(Hide,UEdGraphSchema_K2::PN_Self));W.Lit(Hide,TEXT("InVisibility"),TEXT("Collapsed"));Exec=W.Run(W.P(Close,UEdGraphSchema_K2::PN_Then),Hide);auto* OwnerPC=W.Call(UUserWidget::StaticClass(),TEXT("GetOwningPlayer"));auto* PCGate=W.Branch(Exec,W.Valid(W.Ret(OwnerPC)));auto* Cursor=W.Set(TEXT("bShowMouseCursor"),APlayerController::StaticClass(),W.Ret(OwnerPC));W.Lit(Cursor,TEXT("bShowMouseCursor"),TEXT("false"));Exec=W.Run(W.P(PCGate,UEdGraphSchema_K2::PN_Then),Cursor);auto* GameOnly=W.Call(UWidgetBlueprintLibrary::StaticClass(),TEXT("SetInputMode_GameOnly"));W.Link(W.Ret(OwnerPC),W.P(GameOnly,TEXT("PlayerController")));W.Run(Exec,GameOnly);
 if(!W.OK)return W.Error;FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Widget);FKismetEditorUtilities::CompileBlueprint(Widget);Widget->MarkPackageDirty();if(Widget->Status==BS_Error)return TEXT("tuning widget compile failed");

 }
 UEdGraphPin* Exec=nullptr;
 // Different damping assets change real immediate-physics damping, rather than
 // mixing the output back toward a frozen animation pose. Switching resets the
 // small chain once; subsequent frames retain their state.
 auto* Base=LoadObject<UPhysicsAsset>(nullptr,TEXT("/Game/LeMalin/PA_LeMalin_Braids"));if(!Base)return TEXT("base braid physics missing");
 UPhysicsAsset* Assets[4]={nullptr,Base,nullptr,nullptr};const TCHAR* Suffix[]={TEXT("Light"),TEXT("Standard"),TEXT("Firm"),TEXT("Heavy")};const float Damping[]={.8f,1.6f,2.8f,4.f};
 for(int32 I:{0,2,3}){FString Name=FString(TEXT("PA_LeMalin_Braids_"))+Suffix[I];Assets[I]=Cast<UPhysicsAsset>(StaticDuplicateObject(Base,CreatePackage(*(TEXT("/Game/LeMalin/")+Name)),*Name));if(!Assets[I])return TEXT("damping asset duplication failed");for(USkeletalBodySetup* B:Assets[I]->SkeletalBodySetups)B->DefaultInstance.AngularDamping=Damping[I];Assets[I]->MarkPackageDirty();FAssetRegistryModule::AssetCreated(Assets[I]);}
 UPhysicsAsset* WithoutCollision[4];
 for(int32 I=0;I<4;++I){FString Name=Assets[I]->GetName()+TEXT("_NoCollision");WithoutCollision[I]=Cast<UPhysicsAsset>(StaticDuplicateObject(Assets[I],CreatePackage(*(TEXT("/Game/LeMalin/")+Name)),*Name));if(!WithoutCollision[I])return TEXT("collision toggle asset duplication failed");for(int32 A=0;A<WithoutCollision[I]->SkeletalBodySetups.Num();++A)for(int32 B=A+1;B<WithoutCollision[I]->SkeletalBodySetups.Num();++B)WithoutCollision[I]->DisableCollision(A,B);WithoutCollision[I]->MarkPackageDirty();FAssetRegistryModule::AssetCreated(WithoutCollision[I]);}
 for(const auto& C:Controls)FBlueprintEditorUtils::AddMemberVariable(BP,C.Member,Number,FString::SanitizeFloat(C.Default*C.Factor));
 FEdGraphPinType WidgetType;WidgetType.PinCategory=UEdGraphSchema_K2::PC_Object;WidgetType.PinSubCategoryObject=UUserWidget::StaticClass();FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("LeMalinPanel"),WidgetType);
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
 UEdGraph* E=BP->UbergraphPages[0];Graph G{BP,E};UK2Node_Event* Update=nullptr;for(UEdGraphNode* N:E->Nodes)if(auto* Event=Cast<UK2Node_Event>(N))if(Event->EventReference.GetMemberName()==TEXT("BlueprintUpdateAnimation"))Update=Event;if(!Update)return TEXT("lively update missing");
 auto* Start=G.P(Update,UEdGraphSchema_K2::PN_Then);auto Old=Start->LinkedTo;Start->BreakAllPinLinks();FGraphNodeCreator<UK2Node_ExecutionSequence> SC(*E);auto* Seq=SC.CreateNode();SC.Finalize();Seq->AddInputPin();G.Link(Start,G.P(Seq,UEdGraphSchema_K2::PN_Execute));for(auto* L:Old)G.Link(Seq->GetThenPinGivenIndex(2),L);
 auto* Own=G.Call(UAnimInstance::StaticClass(),TEXT("GetOwningComponent"));auto* Parent=G.Call(USceneComponent::StaticClass(),TEXT("GetAttachParent"));G.Link(G.Ret(Own),G.P(Parent,UEdGraphSchema_K2::PN_Self));auto* Source=G.Cast(G.Ret(Parent),USkeletalMeshComponent::StaticClass());auto* SelectSource=G.Call(UKismetMathLibrary::StaticClass(),TEXT("SelectObject"));G.Link(Source,G.P(SelectSource,TEXT("A")));G.Link(G.Ret(Own),G.P(SelectSource,TEXT("B")));G.Link(G.Valid(Source),G.P(SelectSource,TEXT("bSelectA")));
 auto* SourceOwner=G.Call(UActorComponent::StaticClass(),TEXT("GetOwner"));G.Link(G.Cast(G.Ret(SelectSource),UActorComponent::StaticClass()),G.P(SourceOwner,UEdGraphSchema_K2::PN_Self));auto* Player=G.Call(UGameplayStatics::StaticClass(),TEXT("GetPlayerPawn"));auto* Controller=G.Call(UGameplayStatics::StaticClass(),TEXT("GetPlayerController"));for(auto* N:{Player,Controller})G.Link(G.Ret(Own),G.P(N,TEXT("WorldContextObject")));auto* PC=G.Ret(Controller);auto* Equal=G.Call(UKismetMathLibrary::StaticClass(),TEXT("EqualEqual_ObjectObject"));G.Link(G.Ret(SourceOwner),G.P(Equal,TEXT("A")));G.Link(G.Ret(Player),G.P(Equal,TEXT("B")));auto* PlayerOnly=G.Branch(Seq->GetThenPinGivenIndex(0),G.Ret(Equal));auto* HasPC=G.Branch(G.P(PlayerOnly,UEdGraphSchema_K2::PN_Then),G.Valid(PC));
 auto* HasPanel=G.Branch(G.P(HasPC,UEdGraphSchema_K2::PN_Then),G.Valid(G.Get(TEXT("LeMalinPanel"))));
 auto* Find=G.Call(UWidgetBlueprintLibrary::StaticClass(),TEXT("GetAllWidgetsOfClass"));G.Link(G.Ret(Own),G.P(Find,TEXT("WorldContextObject")));G.Obj(Find,TEXT("WidgetClass"),Widget->GeneratedClass);G.Lit(Find,TEXT("TopLevelOnly"),TEXT("true"));Exec=G.Run(G.P(HasPanel,UEdGraphSchema_K2::PN_Else),Find);
 FGraphNodeCreator<UK2Node_CallArrayFunction> IC(*E);auto* ValidIndex=IC.CreateNode();ValidIndex->SetFromFunction(UKismetArrayLibrary::StaticClass()->FindFunctionByName(TEXT("Array_IsValidIndex")));IC.Finalize();G.Link(G.P(Find,TEXT("FoundWidgets")),G.P(ValidIndex,TEXT("TargetArray")));G.Lit(ValidIndex,TEXT("IndexToTest"),TEXT("0"));auto* Found=G.Branch(Exec,G.Ret(ValidIndex));FGraphNodeCreator<UK2Node_GetArrayItem> ItemC(*E);auto* Item=ItemC.CreateNode();ItemC.Finalize();G.Link(G.P(Find,TEXT("FoundWidgets")),Item->GetTargetArrayPin());auto* Reuse=G.Set(TEXT("LeMalinPanel"));G.Link(Item->GetResultPin(),G.P(Reuse,TEXT("LeMalinPanel")));auto* Reused=G.Run(G.P(Found,UEdGraphSchema_K2::PN_Then),Reuse);
 auto* Create=G.Call(UWidgetBlueprintLibrary::StaticClass(),TEXT("Create"));G.Link(G.Ret(Own),G.P(Create,TEXT("WorldContextObject")));G.Obj(Create,TEXT("WidgetType"),Widget->GeneratedClass);G.Link(PC,G.P(Create,TEXT("OwningPlayer")));Exec=G.Run(G.P(Found,UEdGraphSchema_K2::PN_Else),Create);auto* Store=G.Set(TEXT("LeMalinPanel"));G.Link(G.Ret(Create),G.P(Store,TEXT("LeMalinPanel")));Exec=G.Run(Exec,Store);auto* Add=G.Call(UUserWidget::StaticClass(),TEXT("AddToViewport"));G.Link(G.Get(TEXT("LeMalinPanel")),G.P(Add,UEdGraphSchema_K2::PN_Self));G.Lit(Add,TEXT("ZOrder"),TEXT("100"));Exec=G.Run(Exec,Add);
 auto* Ready=G.Branch(G.P(HasPanel,UEdGraphSchema_K2::PN_Then),G.Valid(G.Get(TEXT("LeMalinPanel"))));G.Link(Exec,G.P(Ready,UEdGraphSchema_K2::PN_Execute));G.Link(Reused,G.P(Ready,UEdGraphSchema_K2::PN_Execute));auto* Typed=G.Cast(G.Get(TEXT("LeMalinPanel")),Widget->GeneratedClass);
 auto* Key=G.Call(APlayerController::StaticClass(),TEXT("WasInputKeyJustPressed"));G.Link(PC,G.P(Key,UEdGraphSchema_K2::PN_Self));FKey F8(EKeys::F8);FString KeyText;FKey::StaticStruct()->ExportText(KeyText,&F8,nullptr,nullptr,PPF_None,nullptr);G.Lit(Key,TEXT("Key"),KeyText);auto* Pressed=G.Branch(G.P(Ready,UEdGraphSchema_K2::PN_Then),G.Ret(Key));auto* PanelInput=G.P(Pressed,UEdGraphSchema_K2::PN_Then);

 auto* Visible=G.Call(UWidget::StaticClass(),TEXT("IsVisible"));auto* Panel=G.Get(TEXT("ControlsPanel"),Widget->GeneratedClass,Typed);G.Link(Panel,G.P(Visible,UEdGraphSchema_K2::PN_Self));auto* Toggle=G.Branch(PanelInput,G.Ret(Visible));
 for(bool Open:{true,false}){auto* SetVisibility=G.Call(UWidget::StaticClass(),TEXT("SetVisibility"));G.Link(Panel,G.P(SetVisibility,UEdGraphSchema_K2::PN_Self));G.Lit(SetVisibility,TEXT("InVisibility"),Open?TEXT("Visible"):TEXT("Collapsed"));auto* T=G.Run(G.P(Toggle,Open?UEdGraphSchema_K2::PN_Else:UEdGraphSchema_K2::PN_Then),SetVisibility);auto* ShowCursor=G.Set(TEXT("bShowMouseCursor"),APlayerController::StaticClass(),PC);G.Lit(ShowCursor,TEXT("bShowMouseCursor"),Open?TEXT("true"):TEXT("false"));T=G.Run(T,ShowCursor);auto* Mode=G.Call(UWidgetBlueprintLibrary::StaticClass(),Open?TEXT("SetInputMode_GameAndUIEx"):TEXT("SetInputMode_GameOnly"));G.Link(PC,G.P(Mode,TEXT("PlayerController")));if(Open){G.Lit(Mode,TEXT("bHideCursorDuringCapture"),TEXT("false"));G.Lit(Mode,TEXT("InMouseLockMode"),TEXT("DoNotLock"));}G.Run(T,Mode);}
 // Read after creation, before the existing blink/ear update. Also permits
 // targeted headless verification using an initialized widget instance.
 auto* ReadGate=G.Branch(Seq->GetThenPinGivenIndex(1),G.Valid(G.Get(TEXT("LeMalinPanel"))));Exec=G.P(ReadGate,UEdGraphSchema_K2::PN_Then);
 for(const auto& C:Controls){
  UEdGraphPin* Value=nullptr;
  if(PlayerRelease)Value=FindLeMalinPlayerSetting(C.Name)?G.Get(C.Name,Widget->GeneratedClass,Typed):G.Number(C.Default);
  else {auto* Slider=G.Call(USlider::StaticClass(),TEXT("GetValue"));G.Link(G.Get(FName(*(FString(C.Name)+TEXT("Slider"))),Widget->GeneratedClass,Typed),G.P(Slider,UEdGraphSchema_K2::PN_Self));Value=G.Ret(Slider);}
  auto* Bound=G.Call(UKismetMathLibrary::StaticClass(),TEXT("FClamp"));G.Link(Value,G.P(Bound,TEXT("Value")));G.Lit(Bound,TEXT("Min"),FString::SanitizeFloat(C.Min));G.Lit(Bound,TEXT("Max"),FString::SanitizeFloat(FString(C.Name)==TEXT("Turn")?0:C.Max));auto* Ratio=G.Call(UKismetMathLibrary::StaticClass(),TEXT("Multiply_DoubleDouble"));G.Link(G.Ret(Bound),G.P(Ratio,TEXT("A")));G.Lit(Ratio,TEXT("B"),FString::SanitizeFloat(C.Factor));auto* Set=G.Set(C.Member);G.Link(G.Ret(Ratio),G.P(Set,C.Member));Exec=G.Run(Exec,Set);}
 if(!G.OK)return G.Error;
 TArray<UEdGraph*> Pages;BP->GetAllGraphs(Pages);UAnimGraphNode_RigidBody* Rigid=nullptr;UAnimGraphNode_RigidBody* Ears=nullptr;UEdGraph* AG=nullptr;UAnimGraphNode_Root* Output=nullptr;
 for(auto* Page:Pages)for(UEdGraphNode* N:Page->Nodes){if(auto* R=Cast<UAnimGraphNode_RigidBody>(N)){if(R->GetTag()==TEXT("LeMalinEarDynamics"))Ears=R;else {Rigid=R;AG=Page;}}if(auto* R=Cast<UAnimGraphNode_Root>(N))Output=R;}
 if(!Rigid||!Ears||!AG||!Output)return TEXT("rigid body graph missing");Graph A{BP,AG};
 auto Expose=[](UAnimGraphNode_SkeletalControlBase* N,FName Name){for(int32 I=0;I<N->ShowPinForProperties.Num();++I)if(N->ShowPinForProperties[I].PropertyName==Name){N->SetPinVisibility(true,I);return;} };
 for(FName Name:{TEXT("SimSpaceSettings"),TEXT("OverrideWorldGravity")})Expose(Rigid,Name);
 auto Multiply=[&](UEdGraphPin* V,double Factor){auto* N=A.Call(UKismetMathLibrary::StaticClass(),TEXT("Multiply_DoubleDouble"));A.Link(V,A.P(N,TEXT("A")));A.Lit(N,TEXT("B"),FString::SanitizeFloat(Factor));return A.Ret(N);};
 FGraphNodeCreator<UK2Node_MakeStruct> MSC(*AG);auto* Settings=MSC.CreateNode();Settings->StructType=FSimSpaceSettings::StaticStruct();MSC.Finalize();
 A.Link(A.Get(TEXT("LeMalinInertia")),A.P(Settings,TEXT("WorldAlpha")));A.Lit(Settings,TEXT("VelocityScaleZ"),TEXT("1"));A.Lit(Settings,TEXT("DampingAlpha"),TEXT("0"));A.Lit(Settings,TEXT("MaxLinearVelocity"),TEXT("1200"));A.Lit(Settings,TEXT("MaxLinearAcceleration"),TEXT("2200"));A.Lit(Settings,TEXT("MaxAngularVelocity"),TEXT("0"));A.Lit(Settings,TEXT("MaxAngularAcceleration"),TEXT("0"));A.Lit(Settings,TEXT("ExternalLinearDragV"),TEXT("0.08,0.08,0"));A.Link(A.P(Settings,FSimSpaceSettings::StaticStruct()->GetFName()),A.P(Rigid,TEXT("SimSpaceSettings")));
 auto* Gravity=A.Call(UKismetMathLibrary::StaticClass(),TEXT("MakeVector"));A.Link(Multiply(A.Get(TEXT("LeMalinGravity")),-980),A.P(Gravity,TEXT("Z")));A.Link(A.Ret(Gravity),A.P(Rigid,TEXT("OverrideWorldGravity")));
 auto SelectAsset=[&](double Threshold,UPhysicsAsset* First,UPhysicsAsset* Second,UEdGraphPin* PinA=nullptr,UEdGraphPin* PinB=nullptr){auto* Less=G.Call(UKismetMathLibrary::StaticClass(),TEXT("Less_DoubleDouble"));G.Link(G.Get(TEXT("LeMalinDamping")),G.P(Less,TEXT("A")));G.Lit(Less,TEXT("B"),FString::SanitizeFloat(Threshold));auto* S=G.Call(UKismetMathLibrary::StaticClass(),TEXT("SelectObject"));if(PinA)G.Link(PinA,G.P(S,TEXT("A")));else G.Obj(S,TEXT("A"),First);if(PinB)G.Link(PinB,G.P(S,TEXT("B")));else G.Obj(S,TEXT("B"),Second);G.Link(G.Ret(Less),G.P(S,TEXT("bSelectA")));return G.Ret(S);};
 auto* Low=SelectAsset(1.5,Assets[0],Assets[1]);auto* High=SelectAsset(3.5,Assets[2],Assets[3]);auto* With=SelectAsset(2.5,nullptr,nullptr,Low,High);
 auto* LowOff=SelectAsset(1.5,WithoutCollision[0],WithoutCollision[1]);auto* HighOff=SelectAsset(3.5,WithoutCollision[2],WithoutCollision[3]);auto* Without=SelectAsset(2.5,nullptr,nullptr,LowOff,HighOff);
 auto* Enabled=G.Call(UKismetMathLibrary::StaticClass(),TEXT("GreaterEqual_DoubleDouble"));G.Link(G.Get(TEXT("LeMalinCollision")),G.P(Enabled,TEXT("A")));G.Lit(Enabled,TEXT("B"),TEXT("0.5"));auto* Pick=G.Call(UKismetMathLibrary::StaticClass(),TEXT("SelectObject"));G.Link(With,G.P(Pick,TEXT("A")));G.Link(Without,G.P(Pick,TEXT("B")));G.Link(G.Ret(Enabled),G.P(Pick,TEXT("bSelectA")));auto* Selected=G.Ret(Pick);
 // Use the native tagged node-reference API: this asset property does not have
 // an optional graph pin in this engine version. No numeric node index or
 // modified engine metadata is required, including after cooking/inheritance.
 Rigid->SetTag(TEXT("LeMalinBraidDynamics"));FGraphNodeCreator<UK2Node> NRC(*E);auto* Reference=CastChecked<UK2Node_AnimNodeReference>(NRC.CreateNode(true,UK2Node_AnimNodeReference::StaticClass()));Reference->SetTag(TEXT("LeMalinBraidDynamics"));NRC.Finalize();
 auto* Convert=G.Call(UAnimNodeRigidBodyLibrary::StaticClass(),TEXT("ConvertToRigidBodyAnimNodePure"));G.Link(G.P(Reference,TEXT("Value")),G.P(Convert,TEXT("Node")));auto* HasNode=G.Branch(Exec,G.P(Convert,TEXT("Result")));
 auto* SetAsset=G.Call(UAnimNodeRigidBodyLibrary::StaticClass(),TEXT("SetOverridePhysicsAsset"));G.Link(G.P(Convert,TEXT("RigidBodyAnimNode")),G.P(SetAsset,TEXT("Node")));G.Link(G.Cast(Selected,UPhysicsAsset::StaticClass()),G.P(SetAsset,TEXT("PhysicsAsset")));Exec=G.Run(G.P(HasNode,UEdGraphSchema_K2::PN_Then),SetAsset);
 auto* Soft=LoadObject<UPhysicsAsset>(nullptr,TEXT("/Game/LeMalin/PA_LeMalin_Ears"));auto* Upright=LoadObject<UPhysicsAsset>(nullptr,TEXT("/Game/LeMalin/PA_LeMalin_Ears_Upright"));if(!Soft||!Upright)return TEXT("ear presets missing");
 auto* EarStyle=G.Call(UKismetMathLibrary::StaticClass(),TEXT("GreaterEqual_DoubleDouble"));G.Link(G.Get(TEXT("LeMalinEarStyle")),G.P(EarStyle,TEXT("A")));G.Lit(EarStyle,TEXT("B"),TEXT("0.5"));auto* EarAsset=G.Call(UKismetMathLibrary::StaticClass(),TEXT("SelectObject"));G.Obj(EarAsset,TEXT("A"),Upright);G.Obj(EarAsset,TEXT("B"),Soft);G.Link(G.Ret(EarStyle),G.P(EarAsset,TEXT("bSelectA")));
 FGraphNodeCreator<UK2Node> ERC(*E);auto* EarReference=CastChecked<UK2Node_AnimNodeReference>(ERC.CreateNode(true,UK2Node_AnimNodeReference::StaticClass()));EarReference->SetTag(TEXT("LeMalinEarDynamics"));ERC.Finalize();auto* EarConvert=G.Call(UAnimNodeRigidBodyLibrary::StaticClass(),TEXT("ConvertToRigidBodyAnimNodePure"));G.Link(G.P(EarReference,TEXT("Value")),G.P(EarConvert,TEXT("Node")));auto* HasEar=G.Branch(Exec,G.P(EarConvert,TEXT("Result")));
 auto* SetEar=G.Call(UAnimNodeRigidBodyLibrary::StaticClass(),TEXT("SetOverridePhysicsAsset"));G.Link(G.P(EarConvert,TEXT("RigidBodyAnimNode")),G.P(SetEar,TEXT("Node")));G.Link(G.Cast(G.Ret(EarAsset),UPhysicsAsset::StaticClass()),G.P(SetEar,TEXT("PhysicsAsset")));G.Run(G.P(HasEar,UEdGraphSchema_K2::PN_Then),SetEar);
 // Uniform visual scale after the braid simulation. The simulation remains in
 // the accepted reference units, and the final pose scales as a whole.
 auto Out=[](UEdGraphNode* N){for(auto* P:N->Pins)if(P->Direction==EGPD_Output)return P;return (UEdGraphPin*)nullptr;};auto* Result=A.P(Output,TEXT("Result"));if(Result->LinkedTo.Num()!=1)return TEXT("panel output pose missing");auto* Before=Result->LinkedTo[0];Result->BreakAllPinLinks();
 FGraphNodeCreator<UAnimGraphNode_LocalToComponentSpace> LC(*AG);auto* Local=LC.CreateNode();LC.Finalize();A.Link(Before,A.P(Local,TEXT("LocalPose")));
 FGraphNodeCreator<UAnimGraphNode_ModifyBone> ScaleC(*AG);auto* Scale=ScaleC.CreateNode();Scale->Node.BoneToModify.BoneName=TEXT("LeMalin");Scale->Node.ScaleMode=BMM_Replace;Scale->Node.ScaleSpace=BCS_ComponentSpace;Scale->Node.Alpha=1;ScaleC.Finalize();Expose(Scale,TEXT("Scale"));auto* V=A.Call(UKismetMathLibrary::StaticClass(),TEXT("MakeVector"));for(const TCHAR* Axis:{TEXT("X"),TEXT("Y"),TEXT("Z")})A.Link(A.Get(TEXT("LeMalinScale")),A.P(V,Axis));A.Link(A.Ret(V),A.P(Scale,TEXT("Scale")));A.Link(Out(Local),A.P(Scale,TEXT("ComponentPose")));FGraphNodeCreator<UAnimGraphNode_ComponentToLocalSpace> CL(*AG);auto* Component=CL.CreateNode();CL.Finalize();A.Link(Out(Scale),A.P(Component,TEXT("ComponentPose")));A.Link(Out(Component),Result);
 if(!A.OK||!G.OK)return A.Error+G.Error;FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);BP->MarkPackageDirty();if(BP->Status==BS_Error)return TEXT("tuning animation compile failed");FString Back=BuildLeMalinBackMount(BP);if(!Back.StartsWith(TEXT("OK")))return Back;if(!PlayerRelease){FString DiagnosticResult=BuildLeMalinBackDiagnostics(BP);if(!DiagnosticResult.StartsWith(TEXT("OK")))return DiagnosticResult;}return BuildLeMalinMaterialTuning(BP);
}
