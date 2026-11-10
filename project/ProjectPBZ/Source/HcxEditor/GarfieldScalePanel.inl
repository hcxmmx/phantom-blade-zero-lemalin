// Uses HcxPanel::Graph, shared with the existing editor-only panel generators.
#include "GameFramework/SaveGame.h"
#include "Engine/BlueprintGeneratedClass.h"

FString UHcxRetargetTools::BuildGarfieldScalePanel(UAnimBlueprint* Blueprint)
{
    using namespace HcxPanel;
    if (!Blueprint) return TEXT("missing Garfield animation blueprint");
    const TCHAR* WidgetPath = TEXT("/Game/Garfield/WBP_GarfieldScale");
    const TCHAR* SavePath = TEXT("/Game/Garfield/SG_GarfieldSettings");
    const FString SlotName = TEXT("Hcxmmx_Garfield_Settings_v1");

    auto* SaveBlueprint = LoadObject<UBlueprint>(nullptr, SavePath);
    if (!SaveBlueprint)
    {
        SaveBlueprint = FKismetEditorUtilities::CreateBlueprint(
            USaveGame::StaticClass(), CreatePackage(SavePath), TEXT("SG_GarfieldSettings"),
            BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
        if (!SaveBlueprint) return TEXT("settings blueprint creation failed");
        FEdGraphPinType NumberType;
        NumberType.PinCategory = UEdGraphSchema_K2::PC_Real;
        NumberType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
        FBlueprintEditorUtils::AddMemberVariable(SaveBlueprint, TEXT("ScalePercent"), NumberType, TEXT("100"));
        for (auto& Variable : SaveBlueprint->NewVariables)
        {
            Variable.PropertyFlags |= CPF_SaveGame;
            Variable.PropertyFlags &= ~CPF_DisableEditOnInstance;
        }
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(SaveBlueprint);
        FKismetEditorUtilities::CompileBlueprint(SaveBlueprint);
        SaveBlueprint->MarkPackageDirty();
    }

    auto* Widget = LoadObject<UWidgetBlueprint>(nullptr, WidgetPath);
    if (Widget) return TEXT("scale panel already exists; restore the baseline before regenerating");
    Widget = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
        UUserWidget::StaticClass(), CreatePackage(WidgetPath), TEXT("WBP_GarfieldScale"),
        BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
    if (!Widget) return TEXT("scale widget creation failed");
    UWidgetTree* Tree = Widget->WidgetTree;
    auto* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
    Tree->RootWidget = Root;
    Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    auto* Panel = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ControlsPanel"));
    Panel->bIsVariable = true;
    Panel->SetBrushColor(FLinearColor(.025f, .03f, .04f, .96f));
    Panel->SetPadding(FMargin(22));
    Panel->SetVisibility(ESlateVisibility::Collapsed);
    auto* PanelSlot = Root->AddChildToCanvas(Panel);
    PanelSlot->SetPosition(FVector2D(32, 120));
    PanelSlot->SetSize(FVector2D(450, 330));
    auto* Body = Tree->ConstructWidget<UVerticalBox>();
    Panel->SetContent(Body);
    auto AddLabel = [&](FName Name, const FString& Text, int32 Size)
    {
        auto* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
        Label->bIsVariable = true;
        Label->SetText(FText::FromString(Text));
        auto Font = Label->GetFont();
        Font.Size = Size;
        Label->SetFont(Font);
        Body->AddChildToVerticalBox(Label)->SetPadding(FMargin(0, 7));
        return Label;
    };
    AddLabel(TEXT("Title"), TEXT("Garfield size / F8"), 24);
    AddLabel(TEXT("Caption"), TEXT("Size (%) / live preview"), 18);
    AddLabel(TEXT("ScaleValue"), TEXT("100"), 22);
    auto* Slider = Tree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("ScaleSlider"));
    Slider->bIsVariable = true;
    Slider->IsFocusable = false;
    Slider->SetMinValue(60);
    Slider->SetMaxValue(110);
    Slider->SetStepSize(1);
    Slider->SetValue(100);
    Body->AddChildToVerticalBox(Slider)->SetPadding(FMargin(0, 12));
    auto* Buttons = Tree->ConstructWidget<UHorizontalBox>();
    Body->AddChildToVerticalBox(Buttons)->SetPadding(FMargin(0, 8));
    for (const auto& Entry : {TPair<FName, FString>(TEXT("ResetButton"), TEXT(" Reset 100% ")),
                             TPair<FName, FString>(TEXT("SaveButton"), TEXT(" Save setting "))})
    {
        auto* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Entry.Key);
        Button->bIsVariable = true;
        FindFProperty<FBoolProperty>(UButton::StaticClass(), TEXT("IsFocusable"))->SetPropertyValue_InContainer(Button, false);
        auto* Label = Tree->ConstructWidget<UTextBlock>();
        Label->SetText(FText::FromString(Entry.Value));
        Button->SetContent(Label);
        Buttons->AddChildToHorizontalBox(Button)->SetPadding(FMargin(0, 0, 12, 0));
    }
    AddLabel(TEXT("Status"), TEXT("F8 closes / Save remembers this size"), 15);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Widget);
    FKismetEditorUtilities::CompileBlueprint(Widget);
    UEdGraph* WidgetGraph = Widget->UbergraphPages[0];
    Graph W{Widget, WidgetGraph};
    auto ReadSlider = [&]()
    {
        auto* Value = W.Call(USlider::StaticClass(), TEXT("GetValue"));
        W.Link(W.Get(TEXT("ScaleSlider")), W.P(Value, UEdGraphSchema_K2::PN_Self));
        return W.Ret(Value);
    };
    auto ClampPercent = [&](UEdGraphPin* Value)
    {
        auto* Clamp = W.Call(UKismetMathLibrary::StaticClass(), TEXT("FClamp"));
        W.Link(Value, W.P(Clamp, TEXT("Value")));
        W.Lit(Clamp, TEXT("Min"), TEXT("60"));
        W.Lit(Clamp, TEXT("Max"), TEXT("110"));
        return W.Ret(Clamp);
    };
    auto Status = [&](UEdGraphPin* Exec, const FString& Message)
    {
        auto* Text = W.Call(UKismetTextLibrary::StaticClass(), TEXT("Conv_StringToText"));
        W.Lit(Text, TEXT("InString"), Message);
        auto* SetText = W.Call(UTextBlock::StaticClass(), TEXT("SetText"));
        W.Link(W.Get(TEXT("Status")), W.P(SetText, UEdGraphSchema_K2::PN_Self));
        W.Link(W.Ret(Text), W.P(SetText, TEXT("InText")));
        return W.Run(Exec, SetText);
    };
    auto* Changed = W.Bound(TEXT("ScaleSlider"), USlider::StaticClass(), TEXT("OnValueChanged"));
    W.Text(W.P(Changed, UEdGraphSchema_K2::PN_Then), TEXT("ScaleValue"), W.P(Changed, TEXT("Value")));
    auto* Reset = W.Bound(TEXT("ResetButton"), UButton::StaticClass(), TEXT("OnClicked"));
    auto* ResetExec = W.Slider(W.P(Reset, UEdGraphSchema_K2::PN_Then), TEXT("ScaleSlider"), W.Number(100));
    W.Text(ResetExec, TEXT("ScaleValue"), W.Number(100));

    auto* Construct = W.Event(UUserWidget::StaticClass(), TEXT("Construct"));
    auto* Exists = W.Call(UGameplayStatics::StaticClass(), TEXT("DoesSaveGameExist"));
    W.Lit(Exists, TEXT("SlotName"), SlotName);
    W.Lit(Exists, TEXT("UserIndex"), TEXT("0"));
    auto* ExistsExec = W.Run(W.P(Construct, UEdGraphSchema_K2::PN_Then), Exists);
    auto* HasSave = W.Branch(ExistsExec, W.Ret(Exists));
    auto* Load = W.Call(UGameplayStatics::StaticClass(), TEXT("LoadGameFromSlot"));
    W.Lit(Load, TEXT("SlotName"), SlotName);
    W.Lit(Load, TEXT("UserIndex"), TEXT("0"));
    auto* LoadExec = W.Run(W.P(HasSave, UEdGraphSchema_K2::PN_Then), Load);
    auto* Loaded = W.Cast(W.Ret(Load), SaveBlueprint->GeneratedClass);
    auto* ValidSave = W.Branch(LoadExec, W.Valid(Loaded));
    auto* SavedValue = ClampPercent(W.Get(TEXT("ScalePercent"), SaveBlueprint->GeneratedClass, Loaded));
    auto* RestoreExec = W.Slider(W.P(ValidSave, UEdGraphSchema_K2::PN_Then), TEXT("ScaleSlider"), SavedValue);
    W.Text(RestoreExec, TEXT("ScaleValue"), SavedValue);

    auto* SaveEvent = W.Bound(TEXT("SaveButton"), UButton::StaticClass(), TEXT("OnClicked"));
    auto* CreateSave = W.Call(UGameplayStatics::StaticClass(), TEXT("CreateSaveGameObject"));
    W.Obj(CreateSave, TEXT("SaveGameClass"), SaveBlueprint->GeneratedClass);
    auto* SaveExec = W.Run(W.P(SaveEvent, UEdGraphSchema_K2::PN_Then), CreateSave);
    auto* NewSave = W.Cast(W.Ret(CreateSave), SaveBlueprint->GeneratedClass);
    auto* StorePercent = W.Set(TEXT("ScalePercent"), SaveBlueprint->GeneratedClass, NewSave);
    W.Link(ClampPercent(ReadSlider()), W.P(StorePercent, TEXT("ScalePercent")));
    SaveExec = W.Run(SaveExec, StorePercent);
    auto* Save = W.Call(UGameplayStatics::StaticClass(), TEXT("SaveGameToSlot"));
    W.Link(NewSave, W.P(Save, TEXT("SaveGameObject")));
    W.Lit(Save, TEXT("SlotName"), SlotName);
    W.Lit(Save, TEXT("UserIndex"), TEXT("0"));
    SaveExec = W.Run(SaveExec, Save);
    auto* Saved = W.Branch(SaveExec, W.Ret(Save));
    Status(W.P(Saved, UEdGraphSchema_K2::PN_Then), TEXT("Saved / restored on the next launch"));
    Status(W.P(Saved, UEdGraphSchema_K2::PN_Else), TEXT("Save failed / current preview still applies"));
    if (!W.OK) return W.Error;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Widget);
    FKismetEditorUtilities::CompileBlueprint(Widget);
    Widget->MarkPackageDirty();
    if (Widget->Status == BS_Error) return TEXT("scale widget compile failed");

    FEdGraphPinType ScaleType;
    ScaleType.PinCategory = UEdGraphSchema_K2::PC_Real;
    ScaleType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
    FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("GarfieldScale"), ScaleType, TEXT("1"));
    FEdGraphPinType WidgetType;
    WidgetType.PinCategory = UEdGraphSchema_K2::PC_Object;
    WidgetType.PinSubCategoryObject = UUserWidget::StaticClass();
    FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("GarfieldPanel"), WidgetType);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    UEdGraph* EventGraph = Blueprint->UbergraphPages[0];
    Graph G{Blueprint, EventGraph};
    UK2Node_Event* Update = nullptr;
    for (UEdGraphNode* Node : EventGraph->Nodes)
        if (auto* Event = Cast<UK2Node_Event>(Node))
            if (Event->EventReference.GetMemberName() == TEXT("BlueprintUpdateAnimation")) Update = Event;
    if (!Update) Update = G.Event(UAnimInstance::StaticClass(), TEXT("BlueprintUpdateAnimation"));
    auto* Own = G.Call(UAnimInstance::StaticClass(), TEXT("GetOwningComponent"));
    auto* Controller = G.Call(UGameplayStatics::StaticClass(), TEXT("GetPlayerController"));
    G.Link(G.Ret(Own), G.P(Controller, TEXT("WorldContextObject")));
    G.Lit(Controller, TEXT("PlayerIndex"), TEXT("0"));
    auto* PC = G.Ret(Controller);
    auto* HasController = G.Branch(G.P(Update, UEdGraphSchema_K2::PN_Then), G.Valid(PC));
    auto* Player = G.Call(UGameplayStatics::StaticClass(), TEXT("GetPlayerPawn"));
    G.Link(G.Ret(Own), G.P(Player, TEXT("WorldContextObject")));
    G.Lit(Player, TEXT("PlayerIndex"), TEXT("0"));
    auto* Owner = G.Call(UAnimInstance::StaticClass(), TEXT("GetOwningActor"));
    auto* IsPlayer = G.Call(UKismetMathLibrary::StaticClass(), TEXT("EqualEqual_ObjectObject"));
    G.Link(G.Ret(Owner), G.P(IsPlayer, TEXT("A")));
    G.Link(G.Ret(Player), G.P(IsPlayer, TEXT("B")));
    auto* PlayerOnly = G.Branch(G.P(HasController, UEdGraphSchema_K2::PN_Then), G.Ret(IsPlayer));
    auto* HasPanel = G.Branch(G.P(PlayerOnly, UEdGraphSchema_K2::PN_Then), G.Valid(G.Get(TEXT("GarfieldPanel"))));
    auto* Find = G.Call(UWidgetBlueprintLibrary::StaticClass(), TEXT("GetAllWidgetsOfClass"));
    G.Link(G.Ret(Own), G.P(Find, TEXT("WorldContextObject")));
    G.Obj(Find, TEXT("WidgetClass"), Widget->GeneratedClass);
    G.Lit(Find, TEXT("TopLevelOnly"), TEXT("true"));
    auto* FindExec = G.Run(G.P(HasPanel, UEdGraphSchema_K2::PN_Else), Find);
    FGraphNodeCreator<UK2Node_CallArrayFunction> ValidIndexCreator(*EventGraph);
    auto* ValidIndex = ValidIndexCreator.CreateNode();
    ValidIndex->SetFromFunction(UKismetArrayLibrary::StaticClass()->FindFunctionByName(TEXT("Array_IsValidIndex")));
    ValidIndexCreator.Finalize();
    G.Link(G.P(Find, TEXT("FoundWidgets")), G.P(ValidIndex, TEXT("TargetArray")));
    G.Lit(ValidIndex, TEXT("IndexToTest"), TEXT("0"));
    auto* Found = G.Branch(FindExec, G.Ret(ValidIndex));
    FGraphNodeCreator<UK2Node_GetArrayItem> ItemCreator(*EventGraph);
    auto* Item = ItemCreator.CreateNode();
    ItemCreator.Finalize();
    G.Link(G.P(Find, TEXT("FoundWidgets")), Item->GetTargetArrayPin());
    Item->GetIndexPin()->DefaultValue = TEXT("0");
    auto* Reuse = G.Set(TEXT("GarfieldPanel"));
    G.Link(Item->GetResultPin(), G.P(Reuse, TEXT("GarfieldPanel")));
    auto* ReuseExec = G.Run(G.P(Found, UEdGraphSchema_K2::PN_Then), Reuse);
    auto* Create = G.Call(UWidgetBlueprintLibrary::StaticClass(), TEXT("Create"));
    G.Link(G.Ret(Own), G.P(Create, TEXT("WorldContextObject")));
    G.Obj(Create, TEXT("WidgetType"), Widget->GeneratedClass);
    G.Link(PC, G.P(Create, TEXT("OwningPlayer")));
    auto* CreateExec = G.Run(G.P(Found, UEdGraphSchema_K2::PN_Else), Create);
    auto* StorePanel = G.Set(TEXT("GarfieldPanel"));
    G.Link(G.Ret(Create), G.P(StorePanel, TEXT("GarfieldPanel")));
    CreateExec = G.Run(CreateExec, StorePanel);
    auto* AddViewport = G.Call(UUserWidget::StaticClass(), TEXT("AddToViewport"));
    G.Link(G.Get(TEXT("GarfieldPanel")), G.P(AddViewport, UEdGraphSchema_K2::PN_Self));
    G.Lit(AddViewport, TEXT("ZOrder"), TEXT("100"));
    CreateExec = G.Run(CreateExec, AddViewport);
    auto* Ready = G.Branch(G.P(HasPanel, UEdGraphSchema_K2::PN_Then), G.Valid(G.Get(TEXT("GarfieldPanel"))));
    G.Link(CreateExec, G.P(Ready, UEdGraphSchema_K2::PN_Execute));
    G.Link(ReuseExec, G.P(Ready, UEdGraphSchema_K2::PN_Execute));
    auto* TypedWidget = G.Cast(G.Get(TEXT("GarfieldPanel")), Widget->GeneratedClass);
    auto* Value = G.Call(USlider::StaticClass(), TEXT("GetValue"));
    G.Link(G.Get(TEXT("ScaleSlider"), Widget->GeneratedClass, TypedWidget), G.P(Value, UEdGraphSchema_K2::PN_Self));
    auto* Ratio = G.Call(UKismetMathLibrary::StaticClass(), TEXT("Multiply_DoubleDouble"));
    G.Link(G.Ret(Value), G.P(Ratio, TEXT("A")));
    G.Lit(Ratio, TEXT("B"), TEXT("0.01"));
    auto* StoreScale = G.Set(TEXT("GarfieldScale"));
    G.Link(G.Ret(Ratio), G.P(StoreScale, TEXT("GarfieldScale")));
    auto* ControlsExec = G.Run(G.P(Ready, UEdGraphSchema_K2::PN_Then), StoreScale);
    auto* Key = G.Call(APlayerController::StaticClass(), TEXT("WasInputKeyJustPressed"));
    G.Link(PC, G.P(Key, UEdGraphSchema_K2::PN_Self));
    FKey F8(EKeys::F8);
    FString KeyText;
    FKey::StaticStruct()->ExportText(KeyText, &F8, nullptr, nullptr, PPF_None, nullptr);
    G.Lit(Key, TEXT("Key"), KeyText);
    auto* Pressed = G.Branch(ControlsExec, G.Ret(Key));
    auto* Controls = G.Get(TEXT("ControlsPanel"), Widget->GeneratedClass, TypedWidget);
    auto* Visible = G.Call(UWidget::StaticClass(), TEXT("IsVisible"));
    G.Link(Controls, G.P(Visible, UEdGraphSchema_K2::PN_Self));
    auto* Toggle = G.Branch(G.P(Pressed, UEdGraphSchema_K2::PN_Then), G.Ret(Visible));
    for (bool Open : {true, false})
    {
        auto* Visibility = G.Call(UWidget::StaticClass(), TEXT("SetVisibility"));
        G.Link(Controls, G.P(Visibility, UEdGraphSchema_K2::PN_Self));
        G.Lit(Visibility, TEXT("InVisibility"), Open ? TEXT("Visible") : TEXT("Collapsed"));
        auto* Exec = G.Run(G.P(Toggle, Open ? UEdGraphSchema_K2::PN_Else : UEdGraphSchema_K2::PN_Then), Visibility);
        auto* Cursor = G.Set(TEXT("bShowMouseCursor"), APlayerController::StaticClass(), PC);
        G.Lit(Cursor, TEXT("bShowMouseCursor"), Open ? TEXT("true") : TEXT("false"));
        Exec = G.Run(Exec, Cursor);
        auto* Mode = G.Call(UWidgetBlueprintLibrary::StaticClass(), Open ? TEXT("SetInputMode_GameAndUIEx") : TEXT("SetInputMode_GameOnly"));
        G.Link(PC, G.P(Mode, TEXT("PlayerController")));
        if (Open)
        {
            G.Lit(Mode, TEXT("bHideCursorDuringCapture"), TEXT("false"));
            G.Lit(Mode, TEXT("InMouseLockMode"), TEXT("DoNotLock"));
        }
        G.Run(Exec, Mode);
    }
    if (!G.OK) return G.Error;

    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    bool ConnectedScale = false;
    for (auto* AnimationGraph : Graphs)
    {
        UAnimGraphNode_Root* Result = nullptr;
        for (UEdGraphNode* Node : AnimationGraph->Nodes)
            if (auto* RootNode = Cast<UAnimGraphNode_Root>(Node)) Result = RootNode;
        if (!Result) continue;
        Graph A{Blueprint, AnimationGraph};
        auto* ResultPose = A.P(Result, TEXT("Result"));
        if (!ResultPose || ResultPose->LinkedTo.Num() != 1) return TEXT("unexpected Garfield output pose");
        auto* Input = ResultPose->LinkedTo[0];
        ResultPose->BreakAllPinLinks();
        auto Output = [](UEdGraphNode* Node)
        {
            for (auto* Pin : Node->Pins) if (Pin->Direction == EGPD_Output) return Pin;
            return static_cast<UEdGraphPin*>(nullptr);
        };
        FGraphNodeCreator<UAnimGraphNode_LocalToComponentSpace> ToComponentCreator(*AnimationGraph);
        auto* ToComponent = ToComponentCreator.CreateNode();
        ToComponentCreator.Finalize();
        A.Link(Input, A.P(ToComponent, TEXT("LocalPose")));
        FGraphNodeCreator<UAnimGraphNode_ModifyBone> ScaleCreator(*AnimationGraph);
        auto* Scale = ScaleCreator.CreateNode();
        Scale->Node.BoneToModify.BoneName = TEXT("Placer");
        Scale->Node.ScaleMode = BMM_Replace;
        Scale->Node.ScaleSpace = BCS_ComponentSpace;
        Scale->Node.Scale = FVector::OneVector;
        Scale->Node.Alpha = 1;
        ScaleCreator.Finalize();
        for (int32 Index = 0; Index < Scale->ShowPinForProperties.Num(); ++Index)
            if (Scale->ShowPinForProperties[Index].PropertyName == TEXT("Scale")) Scale->SetPinVisibility(true, Index);
        auto* Vector = A.Call(UKismetMathLibrary::StaticClass(), TEXT("MakeVector"));
        for (FName Axis : {FName(TEXT("X")), FName(TEXT("Y")), FName(TEXT("Z"))})
            A.Link(A.Get(TEXT("GarfieldScale")), A.P(Vector, Axis));
        A.Link(A.Ret(Vector), A.P(Scale, TEXT("Scale")));
        A.Link(Output(ToComponent), A.P(Scale, TEXT("ComponentPose")));
        FGraphNodeCreator<UAnimGraphNode_ComponentToLocalSpace> ToLocalCreator(*AnimationGraph);
        auto* ToLocal = ToLocalCreator.CreateNode();
        ToLocalCreator.Finalize();
        A.Link(Output(Scale), A.P(ToLocal, TEXT("ComponentPose")));
        A.Link(Output(ToLocal), ResultPose);
        if (!A.OK) return A.Error;
        ConnectedScale = true;
        break;
    }
    if (!ConnectedScale) return TEXT("Garfield animation graph missing");
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    Blueprint->MarkPackageDirty();
    return Blueprint->Status == BS_Error ? TEXT("scale animation compile failed") : TEXT("OK");
}
