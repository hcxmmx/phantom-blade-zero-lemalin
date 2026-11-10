#include "LeMalinNativeUi.inl"
enum class ELeMalinPlayerControl { Choice, Toggle, Distance };

struct FLeMalinPlayerSetting
{
 const TCHAR* Name;
 const TCHAR* Caption;
 ELeMalinPlayerControl Kind;
 double OffValue,OnValue,Default;
 const TCHAR* OffCaption;
 const TCHAR* OnCaption;
};

static const FLeMalinPlayerSetting LeMalinPlayerSettings[]={
 {TEXT("OutfitStyle"),TEXT("服装风格"),ELeMalinPlayerControl::Choice,0,2,0,TEXT("白色"),TEXT("黑色")},
 {TEXT("EarStyle"),TEXT("兔耳模式"),ELeMalinPlayerControl::Choice,0,1,0,TEXT("软耳"),TEXT("竖耳")},
 {TEXT("EyeFinish"),TEXT("眼睛效果"),ELeMalinPlayerControl::Choice,1,3,3,TEXT("一般"),TEXT("宝石")},
 {TEXT("BackVisible"),TEXT("显示背部武器"),ELeMalinPlayerControl::Toggle,0,1,0,TEXT(""),TEXT("")},
 {TEXT("BackDistance"),TEXT("背挂后移距离 · cm"),ELeMalinPlayerControl::Distance,0,40,25,TEXT(""),TEXT("")},
 {TEXT("ExpressionMode"),TEXT("自动表情"),ELeMalinPlayerControl::Toggle,0,4,4,TEXT(""),TEXT("")},
 {TEXT("GazeMode"),TEXT("自动眼神"),ELeMalinPlayerControl::Toggle,0,1,1,TEXT(""),TEXT("")}
};

static const FLeMalinPlayerSetting* FindLeMalinPlayerSetting(FName Name)
{
 for(const auto& Setting:LeMalinPlayerSettings)if(Name==Setting.Name)return &Setting;
 return nullptr;
}

static FString BuildLeMalinPlayerWidget(UWidgetBlueprint* Widget,UBlueprint* SaveBP)
{
 using namespace HcxPanel;
 auto* LabelFont=LeMalinNativeFont(TEXT("ZH_ZiKuTangKuangShanKaiShuJF"));
 auto* BodyFont=LeMalinNativeFont(TEXT("AaGuDianKeBenSong-2"));
 auto* NumberFont=LeMalinNativeFont(TEXT("YingNum"));
 auto Texture=[](const TCHAR* Path){return LoadObject<UTexture2D>(nullptr,Path);};
 auto* PaperTexture=Texture(TEXT("/Game/PBZContent/UI/Menu/StartMenu/T_UI_DifficultySelectButton_Selected"));
 auto* LeftArrow=Texture(TEXT("/Game/PBZContent/UI/Menu/StartMenu/T_SelectArrow_Left"));
 auto* RightArrow=Texture(TEXT("/Game/PBZContent/UI/Menu/StartMenu/T_SelectArrow_Right"));
 auto* LineTexture=Texture(TEXT("/Game/PBZContent/UI/UINew/Hud/T_WhiteLine_Long"));
 auto* Background=Texture(TEXT("/Game/PBZContent/UI/UINew/System/Character/Texture/T_UI_BG"));
 auto* EntryMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/PBZContent/UI/Menu/Art/MI_UI_Settings_EntryBorder"));
 if(!LabelFont||!BodyFont||!NumberFont||!PaperTexture||!LeftArrow||!RightArrow||!LineTexture||!Background||!EntryMaterial)return TEXT("native UI resource missing");
 const FLinearColor Paper(.9f,.88f,.8f,1),Ink(.015f,.015f,.012f,1),Muted(.6f,.6f,.55f,1);
 UWidgetTree* Tree=Widget->WidgetTree;
 auto* Root=Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("Root"));
 Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);Tree->RootWidget=Root;
 auto* Border=Tree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("ControlsPanel"));
 Border->bIsVariable=true;Border->SetBrush(LeMalinNativeBrush(Background,FVector2D(560,760)));Border->SetBrushColor(FLinearColor::White);
 Border->SetPadding(FMargin(30,25));Border->SetVisibility(ESlateVisibility::Collapsed);
 auto* Position=Root->AddChildToCanvas(Border);
 Position->SetAnchors(FAnchors(.94f,.5f));Position->SetAlignment(FVector2D(1,.5));
 Position->SetPosition(FVector2D::ZeroVector);Position->SetSize(FVector2D(560,800));
 auto* Body=Tree->ConstructWidget<UVerticalBox>();Border->SetContent(Body);
 auto Label=[&](FName Name,const FString& Text,int32 Size,FLinearColor Color){
  auto* TextBlock=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);
  TextBlock->bIsVariable=true;TextBlock->SetText(FText::FromString(Text));
  auto Font=TextBlock->GetFont();Font.FontObject=Size>=18?LabelFont:BodyFont;
  if(Name==TEXT("BackDistanceValue"))Font.FontObject=NumberFont;
  Font.Size=Size;Font.TypefaceFontName=TEXT("Default");Font.LetterSpacing=10;TextBlock->SetFont(Font);
  TextBlock->SetShadowOffset(FVector2D(1,1));TextBlock->SetShadowColorAndOpacity(FLinearColor(0,0,0,.8f));
  TextBlock->SetColorAndOpacity(Color);return TextBlock;
 };
 auto AddLabel=[&](FName Name,const FString& Text,int32 Size,FLinearColor Color,FMargin Padding){
  auto* TextBlock=Label(Name,Text,Size,Color);Body->AddChildToVerticalBox(TextBlock)->SetPadding(Padding);return TextBlock;
 };
 auto Rule=[&](){
  auto* Line=Tree->ConstructWidget<UImage>();Line->SetBrush(LeMalinNativeBrush(LineTexture,FVector2D(500,12)));
  auto* Height=Tree->ConstructWidget<USizeBox>();Height->SetHeightOverride(8);Height->SetContent(Line);
  Body->AddChildToVerticalBox(Height)->SetPadding(FMargin(0,10));
 };
 auto Button=[&](FName Name,const FString& Caption,int32 FontSize,bool Filled){
  auto* Result=Tree->ConstructWidget<UButton>(UButton::StaticClass(),Name);Result->bIsVariable=true;
  FindFProperty<FBoolProperty>(UButton::StaticClass(),TEXT("IsFocusable"))->SetPropertyValue_InContainer(Result,false);
  auto Style=Result->GetStyle();
  Style.Normal=LeMalinNativeBrush(PaperTexture,FVector2D(160,42));
  if(!Filled)Style.Normal.TintColor=FLinearColor(1,1,1,.35f);
  Style.Hovered=LeMalinNativeBrush(PaperTexture,FVector2D(160,42));Style.Pressed=Style.Hovered;Style.Pressed.TintColor=FLinearColor(.65f,.62f,.55f,1);
  Style.NormalPadding=FMargin(9,6);Style.PressedPadding=Style.NormalPadding;Result->SetStyle(Style);
  if(Filled)Result->SetContent(Label(NAME_None,Caption,FontSize,Ink));
  else{
   auto* Image=Tree->ConstructWidget<UImage>();Image->SetBrush(LeMalinNativeBrush(Caption==TEXT("‹")?LeftArrow:RightArrow,FVector2D(24,24)));
   Result->SetContent(Image);
  }
  return Result;
 };
 AddLabel(TEXT("Eyebrow"),TEXT("LE MALIN  /  角色外观"),12,Muted,FMargin(0,0,0,4));
 AddLabel(TEXT("Title"),TEXT("恶毒"),36,Paper,FMargin(0,0,0,4));
 AddLabel(TEXT("Hint"),TEXT("F8  打开 / 收起设置"),13,Muted,FMargin(0,0,0,2));
 Rule();
 for(const auto& Setting:LeMalinPlayerSettings){
  FString Name=Setting.Name;
  if(Name==TEXT("BackVisible")||Name==TEXT("ExpressionMode"))Rule();
  auto* Row=Tree->ConstructWidget<UHorizontalBox>();
  auto* Entry=Tree->ConstructWidget<UButton>();
  FindFProperty<FBoolProperty>(UButton::StaticClass(),TEXT("IsFocusable"))->SetPropertyValue_InContainer(Entry,false);
  auto EntryStyle=Entry->GetStyle();EntryStyle.Normal=LeMalinNativeBrush(EntryMaterial,FVector2D(500,44));
  EntryStyle.Hovered=Setting.Kind==ELeMalinPlayerControl::Choice?LeMalinNativeBrush(PaperTexture,FVector2D(500,44)):EntryStyle.Normal;
  EntryStyle.Pressed=EntryStyle.Hovered;EntryStyle.NormalForeground=Paper;EntryStyle.HoveredForeground=Ink;EntryStyle.PressedForeground=Ink;
  EntryStyle.NormalPadding=EntryStyle.PressedPadding=FMargin(6,2);Entry->SetStyle(EntryStyle);Entry->SetContent(Row);
  CastChecked<UButtonSlot>(Row->Slot)->SetHorizontalAlignment(HAlign_Fill);
  auto* RowSlot=Body->AddChildToVerticalBox(Entry);RowSlot->SetPadding(FMargin(0,6));
  auto* Caption=Label(FName(*(Name+TEXT("Caption"))),Setting.Caption,20,Paper);
  if(Setting.Kind==ELeMalinPlayerControl::Choice)Caption->SetColorAndOpacity(FSlateColor::UseForeground());
  auto* CaptionSlot=Row->AddChildToHorizontalBox(Caption);
  CaptionSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));CaptionSlot->SetVerticalAlignment(VAlign_Center);
  auto* Width=Tree->ConstructWidget<USizeBox>();Width->SetWidthOverride(184);Width->SetHeightOverride(38);Row->AddChildToHorizontalBox(Width);
  if(Setting.Kind==ELeMalinPlayerControl::Choice){
   auto* Choice=Tree->ConstructWidget<UHorizontalBox>();Width->SetContent(Choice);
   Choice->AddChildToHorizontalBox(Button(FName(*(Name+TEXT("Previous"))),TEXT("‹"),18,false));
   auto* Value=Label(FName(*(Name+TEXT("Value"))),Setting.Default==Setting.OnValue?Setting.OnCaption:Setting.OffCaption,18,Paper);
   Value->SetColorAndOpacity(FSlateColor::UseForeground());
   Value->SetJustification(ETextJustify::Center);auto* ValueSlot=Choice->AddChildToHorizontalBox(Value);
   ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));ValueSlot->SetVerticalAlignment(VAlign_Center);
   Choice->AddChildToHorizontalBox(Button(FName(*(Name+TEXT("Next"))),TEXT("›"),18,false));
  }else if(Setting.Kind==ELeMalinPlayerControl::Toggle){
   auto* Check=Tree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(),FName(*(Name+TEXT("Check"))));
   Check->bIsVariable=true;Check->SetIsChecked(Setting.Default==Setting.OnValue);
   FindFProperty<FBoolProperty>(UCheckBox::StaticClass(),TEXT("IsFocusable"))->SetPropertyValue_InContainer(Check,false);
   auto Style=Check->GetWidgetStyle();
   Style.CheckedImage.TintColor=Paper;Style.CheckedHoveredImage.TintColor=Paper;Style.CheckedPressedImage.TintColor=Paper;
   for(auto* Image:{&Style.CheckedImage,&Style.CheckedHoveredImage,&Style.CheckedPressedImage,&Style.UncheckedImage,&Style.UncheckedHoveredImage,&Style.UncheckedPressedImage})Image->ImageSize=FVector2D(24,24);
   Check->SetWidgetStyle(Style);Check->HorizontalAlignment=HAlign_Right;Width->SetContent(Check);
   CastChecked<USizeBoxSlot>(Check->Slot)->SetHorizontalAlignment(HAlign_Center);
  }else{
   auto* Value=Label(TEXT("BackDistanceValue"),TEXT("25"),18,Paper);Value->SetJustification(ETextJustify::Right);Width->SetContent(Value);
   auto* Slider=Tree->ConstructWidget<USlider>(USlider::StaticClass(),TEXT("BackDistanceSlider"));
   Slider->bIsVariable=true;Slider->IsFocusable=false;Slider->MouseUsesStep=true;
   Slider->SetMinValue(0);Slider->SetMaxValue(40);Slider->SetStepSize(1);Slider->SetValue(25);
   Slider->SetSliderBarColor(Muted);Slider->SetSliderHandleColor(Paper);
   Body->AddChildToVerticalBox(Slider)->SetPadding(FMargin(0,0,0,6));
  }
 }
 auto* Spacer=Tree->ConstructWidget<USizeBox>();Body->AddChildToVerticalBox(Spacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
 Rule();
 auto* Footer=Tree->ConstructWidget<UHorizontalBox>();Body->AddChildToVerticalBox(Footer)->SetPadding(FMargin(0,5));
 for(auto Entry:{TPair<FName,FString>(TEXT("ResetButton"),TEXT("恢复默认")),{TEXT("SaveButton"),TEXT("保存设置")},{TEXT("CloseButton"),TEXT("收起")}}){
  auto* Slot=Footer->AddChildToHorizontalBox(Button(Entry.Key,Entry.Value,16,true));
  Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Slot->SetPadding(FMargin(0,0,Entry.Key==TEXT("CloseButton")?0:10,0));
 }
 AddLabel(TEXT("Status"),TEXT("调整即时生效，保存后下次沿用。"),13,Muted,FMargin(0,8,0,0));
 AddLabel(TEXT("VoiceHint"),TEXT("语音为独立可选安装包"),12,Muted,FMargin(0,6,0,0));

 FEdGraphPinType Number;Number.PinCategory=UEdGraphSchema_K2::PC_Real;Number.PinSubCategory=UEdGraphSchema_K2::PC_Double;
 for(const auto& Setting:LeMalinPlayerSettings)FBlueprintEditorUtils::AddMemberVariable(Widget,Setting.Name,Number,FString::SanitizeFloat(Setting.Default));
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Widget);FKismetEditorUtilities::CompileBlueprint(Widget);
 Graph G{Widget,Widget->UbergraphPages[0]};
 auto Math=[&](FName Function,UEdGraphPin* A,UEdGraphPin* B){
  auto* Node=G.Call(UKismetMathLibrary::StaticClass(),Function);G.Link(A,G.P(Node,TEXT("A")));G.Link(B,G.P(Node,TEXT("B")));return G.Ret(Node);
 };
 auto On=[&](const FLeMalinPlayerSetting& Setting,UEdGraphPin* Value){return Math(TEXT("GreaterEqual_DoubleDouble"),Value,G.Number((Setting.OffValue+Setting.OnValue)*.5));};
 auto Pick=[&](UEdGraphPin* Condition,double A,double B){
  auto* Node=G.Call(UKismetMathLibrary::StaticClass(),TEXT("SelectFloat"));
  G.Link(Condition,G.P(Node,TEXT("bPickA")));G.Lit(Node,TEXT("A"),FString::SanitizeFloat(A));G.Lit(Node,TEXT("B"),FString::SanitizeFloat(B));return G.Ret(Node);
 };
 auto Normalize=[&](const FLeMalinPlayerSetting& Setting,UEdGraphPin* Value){
  if(Setting.Kind!=ELeMalinPlayerControl::Distance)return Pick(On(Setting,Value),Setting.OnValue,Setting.OffValue);
  auto* Clamp=G.Call(UKismetMathLibrary::StaticClass(),TEXT("FClamp"));G.Link(Value,G.P(Clamp,TEXT("Value")));G.Lit(Clamp,TEXT("Min"),TEXT("0"));G.Lit(Clamp,TEXT("Max"),TEXT("40"));return G.Ret(Clamp);
 };
 auto SetText=[&](UEdGraphPin* Exec,FName Name,UEdGraphPin* Text){
  auto* Convert=G.Call(UKismetTextLibrary::StaticClass(),TEXT("Conv_StringToText"));G.Link(Text,G.P(Convert,TEXT("InString")));
  auto* Apply=G.Call(UTextBlock::StaticClass(),TEXT("SetText"));G.Link(G.Get(Name),G.P(Apply,UEdGraphSchema_K2::PN_Self));G.Link(G.Ret(Convert),G.P(Apply,TEXT("InText")));return G.Run(Exec,Apply);
 };
 auto Status=[&](UEdGraphPin* Exec,const TCHAR* Message){
  auto* Literal=G.Call(UKismetSystemLibrary::StaticClass(),TEXT("MakeLiteralString"));G.Lit(Literal,TEXT("Value"),Message);return SetText(Exec,TEXT("Status"),G.Ret(Literal));
 };
 auto Refresh=[&](UEdGraphPin* Exec,const FLeMalinPlayerSetting& Setting){
  FString Name=Setting.Name;auto* Value=G.Get(Setting.Name);
  if(Setting.Kind==ELeMalinPlayerControl::Choice){
   auto* Choice=G.Call(UKismetMathLibrary::StaticClass(),TEXT("SelectString"));G.Link(On(Setting,Value),G.P(Choice,TEXT("bPickA")));
   G.Lit(Choice,TEXT("A"),Setting.OnCaption);G.Lit(Choice,TEXT("B"),Setting.OffCaption);return SetText(Exec,FName(*(Name+TEXT("Value"))),G.Ret(Choice));
  }
  if(Setting.Kind==ELeMalinPlayerControl::Toggle){
   auto* Check=G.Call(UCheckBox::StaticClass(),TEXT("SetIsChecked"));G.Link(G.Get(FName(*(Name+TEXT("Check")))),G.P(Check,UEdGraphSchema_K2::PN_Self));G.Link(On(Setting,Value),G.P(Check,TEXT("InIsChecked")));return G.Run(Exec,Check);
  }
  Exec=G.Slider(Exec,TEXT("BackDistanceSlider"),Value);
  return G.Text(Exec,TEXT("BackDistanceValue"),Value);
 };
 auto Store=[&](UEdGraphPin* Exec,const FLeMalinPlayerSetting& Setting,UEdGraphPin* Value){
  auto* Assign=G.Set(Setting.Name);G.Link(Value,G.P(Assign,Setting.Name));return Refresh(G.Run(Exec,Assign),Setting);
 };
 for(const auto& Setting:LeMalinPlayerSettings){
  FString Name=Setting.Name;
  if(Setting.Kind==ELeMalinPlayerControl::Choice){
   for(const TCHAR* Suffix:{TEXT("Previous"),TEXT("Next")}){
    auto* Event=G.Bound(FName(*(Name+Suffix)),UButton::StaticClass(),TEXT("OnClicked"));
    auto* Exec=Store(G.P(Event,UEdGraphSchema_K2::PN_Then),Setting,Pick(On(Setting,G.Get(Setting.Name)),Setting.OffValue,Setting.OnValue));
    Status(Exec,TEXT("调整已生效，点击保存可记住。"));
   }
  }else if(Setting.Kind==ELeMalinPlayerControl::Toggle){
   auto* Event=G.Bound(FName(*(Name+TEXT("Check"))),UCheckBox::StaticClass(),TEXT("OnCheckStateChanged"));
   auto* Exec=Store(G.P(Event,UEdGraphSchema_K2::PN_Then),Setting,Pick(G.P(Event,TEXT("bIsChecked")),Setting.OnValue,Setting.OffValue));
   Status(Exec,TEXT("调整已生效，点击保存可记住。"));
  }else{
   auto* Event=G.Bound(TEXT("BackDistanceSlider"),USlider::StaticClass(),TEXT("OnValueChanged"));
   auto* Exec=Store(G.P(Event,UEdGraphSchema_K2::PN_Then),Setting,Normalize(Setting,G.P(Event,TEXT("Value"))));
   Status(Exec,TEXT("调整已生效，点击保存可记住。"));
  }
 }
 auto* Reset=G.Bound(TEXT("ResetButton"),UButton::StaticClass(),TEXT("OnClicked"));auto* Exec=G.P(Reset,UEdGraphSchema_K2::PN_Then);
 for(const auto& Setting:LeMalinPlayerSettings)Exec=Store(Exec,Setting,G.Number(Setting.Default));
 Status(Exec,TEXT("已恢复默认，点击保存可记住。"));
 auto* Construct=G.Event(UUserWidget::StaticClass(),TEXT("Construct"));
 auto* Exists=G.Call(UGameplayStatics::StaticClass(),TEXT("DoesSaveGameExist"));G.Link(G.Get(TEXT("SettingsSlot")),G.P(Exists,TEXT("SlotName")));
 auto* HasSave=G.Branch(G.Run(G.P(Construct,UEdGraphSchema_K2::PN_Then),Exists),G.Ret(Exists));
 auto* Load=G.Call(UGameplayStatics::StaticClass(),TEXT("LoadGameFromSlot"));G.Link(G.Get(TEXT("SettingsSlot")),G.P(Load,TEXT("SlotName")));
 auto* Loaded=G.Cast(G.Ret(Load),SaveBP->GeneratedClass);
 auto* Valid=G.Branch(G.Run(G.P(HasSave,UEdGraphSchema_K2::PN_Then),Load),G.Valid(Loaded));Exec=G.P(Valid,UEdGraphSchema_K2::PN_Then);
 for(const auto& Setting:LeMalinPlayerSettings)Exec=Store(Exec,Setting,Normalize(Setting,G.Get(Setting.Name,SaveBP->GeneratedClass,Loaded)));
 Status(Exec,TEXT("已载入保存的设置。"));
 Status(G.P(Valid,UEdGraphSchema_K2::PN_Else),TEXT("设置读取失败，当前采用默认值。"));
 auto* SaveEvent=G.Bound(TEXT("SaveButton"),UButton::StaticClass(),TEXT("OnClicked"));
 auto* Create=G.Call(UGameplayStatics::StaticClass(),TEXT("CreateSaveGameObject"));G.Obj(Create,TEXT("SaveGameClass"),SaveBP->GeneratedClass);
 Exec=G.Run(G.P(SaveEvent,UEdGraphSchema_K2::PN_Then),Create);auto* SaveObject=G.Cast(G.Ret(Create),SaveBP->GeneratedClass);
 for(const auto& Setting:LeMalinPlayerSettings){
  auto* Assign=G.Set(Setting.Name,SaveBP->GeneratedClass,SaveObject);G.Link(Normalize(Setting,G.Get(Setting.Name)),G.P(Assign,Setting.Name));Exec=G.Run(Exec,Assign);
 }
 auto* Save=G.Call(UGameplayStatics::StaticClass(),TEXT("SaveGameToSlot"));G.Link(SaveObject,G.P(Save,TEXT("SaveGameObject")));G.Link(G.Get(TEXT("SettingsSlot")),G.P(Save,TEXT("SlotName")));
 auto* Saved=G.Branch(G.Run(Exec,Save),G.Ret(Save));
 Status(G.P(Saved,UEdGraphSchema_K2::PN_Then),TEXT("设置已保存。"));Status(G.P(Saved,UEdGraphSchema_K2::PN_Else),TEXT("保存失败，本次调整仍有效。"));
 auto* Close=G.Bound(TEXT("CloseButton"),UButton::StaticClass(),TEXT("OnClicked"));
 auto* Hide=G.Call(UWidget::StaticClass(),TEXT("SetVisibility"));G.Link(G.Get(TEXT("ControlsPanel")),G.P(Hide,UEdGraphSchema_K2::PN_Self));G.Lit(Hide,TEXT("InVisibility"),TEXT("Collapsed"));Exec=G.Run(G.P(Close,UEdGraphSchema_K2::PN_Then),Hide);
 auto* Owner=G.Call(UUserWidget::StaticClass(),TEXT("GetOwningPlayer"));auto* HasOwner=G.Branch(Exec,G.Valid(G.Ret(Owner)));
 auto* Cursor=G.Set(TEXT("bShowMouseCursor"),APlayerController::StaticClass(),G.Ret(Owner));G.Lit(Cursor,TEXT("bShowMouseCursor"),TEXT("false"));Exec=G.Run(G.P(HasOwner,UEdGraphSchema_K2::PN_Then),Cursor);
 auto* Game=G.Call(UWidgetBlueprintLibrary::StaticClass(),TEXT("SetInputMode_GameOnly"));G.Link(G.Ret(Owner),G.P(Game,TEXT("PlayerController")));G.Run(Exec,Game);
 if(!G.OK)return G.Error;
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Widget);FKismetEditorUtilities::CompileBlueprint(Widget);Widget->MarkPackageDirty();
 return Widget->Status==BS_Error?TEXT("player settings widget compile failed"):TEXT("OK");
}
