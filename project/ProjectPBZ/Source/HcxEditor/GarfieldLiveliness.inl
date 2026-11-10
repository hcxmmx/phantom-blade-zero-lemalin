// Editor-only construction of engine-native blink and tail animation nodes.
static void AddGarfieldOpenEyeMorph(HcxPanel::Graph& G, UK2Node_CallFunction* LastMorph)
{
    auto* Stage = G.Call(UKismetMathLibrary::StaticClass(), TEXT("Multiply_DoubleDouble"));
    G.Link(G.Get(TEXT("BlinkProgress")), G.P(Stage, TEXT("A")));
    G.Lit(Stage, TEXT("B"), TEXT("4"));
    auto* Remaining = G.Call(UKismetMathLibrary::StaticClass(), TEXT("Subtract_DoubleDouble"));
    G.Lit(Remaining, TEXT("A"), TEXT("1"));
    G.Link(G.Ret(Stage), G.P(Remaining, TEXT("B")));
    auto* Clamp = G.Call(UKismetMathLibrary::StaticClass(), TEXT("FClamp"));
    G.Link(G.Ret(Remaining), G.P(Clamp, TEXT("Value")));
    G.Lit(Clamp, TEXT("Min"), TEXT("0")); G.Lit(Clamp, TEXT("Max"), TEXT("1"));
    auto* Open = G.Call(UAnimInstance::StaticClass(), TEXT("SetMorphTarget"));
    G.Lit(Open, TEXT("MorphTargetName"), TEXT("EyesOpen"));
    G.Link(G.Ret(Clamp), G.P(Open, TEXT("Value")));
    auto* Then = G.P(LastMorph, UEdGraphSchema_K2::PN_Then);
    auto Existing = Then->LinkedTo; Then->BreakAllPinLinks();
    auto* End = G.Run(Then, Open);
    for (auto* Link : Existing) G.Link(End, Link);
}

FString UHcxRetargetTools::BuildGarfieldLiveliness(UAnimBlueprint* Blueprint)
{
    using namespace HcxPanel;
    if (!Blueprint) return TEXT("missing Garfield blueprint");
    if (FindFProperty<FDoubleProperty>(Blueprint->GeneratedClass, TEXT("BlinkClock")))
    {
        TArray<UEdGraph*> ExistingGraphs;
        Blueprint->GetAllGraphs(ExistingGraphs);
        int32 MorphCalls = 0, TailNodes = 0;
        for (UEdGraph* Graph : ExistingGraphs)
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                if (auto* Call = Cast<UK2Node_CallFunction>(Node))
                    MorphCalls += Call->FunctionReference.GetMemberName() == TEXT("SetMorphTarget");
                if (auto* Bone = Cast<UAnimGraphNode_ModifyBone>(Node))
                    TailNodes += Bone->Node.BoneToModify.BoneName.ToString().StartsWith(TEXT("tail"));
            }
        if ((MorphCalls != 4 && MorphCalls != 5) || TailNodes != 3)
            return TEXT("incomplete liveliness graph; restore baseline");
        for (UEdGraph* Page : ExistingGraphs)
        {
            Graph G{Blueprint, Page};
            auto Nodes = Page->Nodes;
            for (UEdGraphNode* Node : Nodes)
            {
                if (auto* Call = Cast<UK2Node_CallFunction>(Node))
                    if (MorphCalls == 4 && Call->FunctionReference.GetMemberName() == TEXT("SetMorphTarget"))
                        if (G.P(Call, TEXT("MorphTargetName"))->DefaultValue == TEXT("Blink100")) AddGarfieldOpenEyeMorph(G, Call);
                if (auto* Bone = Cast<UAnimGraphNode_ModifyBone>(Node))
                {
                    const FString Name = Bone->Node.BoneToModify.BoneName.ToString();
                    if (!Name.StartsWith(TEXT("tail"))) continue;
                    auto* Rotation = G.P(Bone, TEXT("Rotation"));
                    if (Rotation->LinkedTo.Num() != 1) return TEXT("tail rotation source missing");
                    auto* Yaw = G.P(Rotation->LinkedTo[0]->GetOwningNode(), TEXT("Yaw"));
                    if (Yaw->LinkedTo.Num() != 1) return TEXT("tail amplitude source missing");
                    const double Amplitude = Name == TEXT("tail1") ? 8 : Name == TEXT("tail3") ? 12 : 16;
                    G.Lit(Yaw->LinkedTo[0]->GetOwningNode(), TEXT("B"), FString::SanitizeFloat(Amplitude));
                }
            }
            if (!G.OK) return G.Error;
        }
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        Blueprint->MarkPackageDirty();
        return Blueprint->Status == BS_Error ? TEXT("blink topology fix compile failed") : TEXT("OK");
    }
    FEdGraphPinType Number;
    Number.PinCategory = UEdGraphSchema_K2::PC_Real;
    Number.PinSubCategory = UEdGraphSchema_K2::PC_Double;
    for (const auto& Variable : {TPair<FName, FString>(TEXT("LifeClock"), TEXT("0")),
                                TPair<FName, FString>(TEXT("BlinkClock"), TEXT("-3.5")),
                                TPair<FName, FString>(TEXT("BlinkProgress"), TEXT("0"))})
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, Variable.Key, Number, Variable.Value);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    UEdGraph* EventGraph = Blueprint->UbergraphPages[0];
    Graph G{Blueprint, EventGraph};
    UK2Node_Event* Update = nullptr;
    for (UEdGraphNode* Node : EventGraph->Nodes)
        if (auto* Event = Cast<UK2Node_Event>(Node))
            if (Event->EventReference.GetMemberName() == TEXT("BlueprintUpdateAnimation")) Update = Event;
    if (!Update) return TEXT("animation update missing");
    auto Math = [&](FName Function, UEdGraphPin* A, UEdGraphPin* B)
    {
        auto* Node = G.Call(UKismetMathLibrary::StaticClass(), Function);
        G.Link(A, G.P(Node, TEXT("A")));
        G.Link(B, G.P(Node, TEXT("B")));
        return G.Ret(Node);
    };
    auto Clamp = [&](UEdGraphPin* Value)
    {
        auto* Node = G.Call(UKismetMathLibrary::StaticClass(), TEXT("FClamp"));
        G.Link(Value, G.P(Node, TEXT("Value")));
        G.Lit(Node, TEXT("Min"), TEXT("0"));
        G.Lit(Node, TEXT("Max"), TEXT("1"));
        return G.Ret(Node);
    };
    auto* EventExec = G.P(Update, UEdGraphSchema_K2::PN_Then);
    auto Previous = EventExec->LinkedTo;
    EventExec->BreakAllPinLinks();
    FGraphNodeCreator<UK2Node_ExecutionSequence> SequenceCreator(*EventGraph);
    auto* Sequence = SequenceCreator.CreateNode();
    SequenceCreator.Finalize();
    G.Link(EventExec, G.P(Sequence, UEdGraphSchema_K2::PN_Execute));
    for (auto* Link : Previous) G.Link(Sequence->GetThenPinGivenIndex(0), Link);
    auto* Delta = G.P(Update, TEXT("DeltaTimeX"));
    auto* SetTime = G.Set(TEXT("LifeClock"));
    G.Link(Math(TEXT("Add_DoubleDouble"), G.Get(TEXT("LifeClock")), Delta), G.P(SetTime, TEXT("LifeClock")));
    auto* Exec = G.Run(Sequence->GetThenPinGivenIndex(1), SetTime);
    auto* SetBlink = G.Set(TEXT("BlinkClock"));
    G.Link(Math(TEXT("Add_DoubleDouble"), G.Get(TEXT("BlinkClock")), Delta), G.P(SetBlink, TEXT("BlinkClock")));
    Exec = G.Run(Exec, SetBlink);
    auto* Finished = Math(TEXT("GreaterEqual_DoubleDouble"), G.Get(TEXT("BlinkClock")), G.Number(.32));
    auto* Reset = G.Branch(Exec, Finished);
    auto* RandomWait = G.Call(UKismetMathLibrary::StaticClass(), TEXT("RandomFloatInRange"));
    G.Lit(RandomWait, TEXT("Min"), TEXT("3"));
    G.Lit(RandomWait, TEXT("Max"), TEXT("6"));
    auto* ResetClock = G.Set(TEXT("BlinkClock"));
    G.Link(Math(TEXT("Multiply_DoubleDouble"), G.Ret(RandomWait), G.Number(-1)), G.P(ResetClock, TEXT("BlinkClock")));
    auto* ResetExec = G.Run(G.P(Reset, UEdGraphSchema_K2::PN_Then), ResetClock);
    auto* Closing = Math(TEXT("Divide_DoubleDouble"), G.Get(TEXT("BlinkClock")), G.Number(.1));
    auto* Opening = Math(TEXT("Divide_DoubleDouble"), Math(TEXT("Subtract_DoubleDouble"), G.Number(.32), G.Get(TEXT("BlinkClock"))), G.Number(.18));
    auto* Progress = G.Set(TEXT("BlinkProgress"));
    G.Link(Clamp(Math(TEXT("FMin"), Closing, Opening)), G.P(Progress, TEXT("BlinkProgress")));
    G.Link(ResetExec, G.P(Progress, UEdGraphSchema_K2::PN_Execute));
    Exec = G.Run(G.P(Reset, UEdGraphSchema_K2::PN_Else), Progress);
    auto* Stage = Math(TEXT("Multiply_DoubleDouble"), G.Get(TEXT("BlinkProgress")), G.Number(4));
    UK2Node_CallFunction* LastMorph = nullptr;
    for (int32 Index = 1; Index <= 4; ++Index)
    {
        auto* Distance = G.Call(UKismetMathLibrary::StaticClass(), TEXT("Abs"));
        G.Link(Math(TEXT("Subtract_DoubleDouble"), Stage, G.Number(Index)), G.P(Distance, TEXT("A")));
        auto* Weight = Clamp(Math(TEXT("Subtract_DoubleDouble"), G.Number(1), G.Ret(Distance)));
        auto* Morph = G.Call(UAnimInstance::StaticClass(), TEXT("SetMorphTarget"));
        G.Lit(Morph, TEXT("MorphTargetName"), FString::Printf(TEXT("Blink%d"), Index * 25));
        G.Link(Weight, G.P(Morph, TEXT("Value")));
        Exec = G.Run(Exec, Morph);
        LastMorph = Morph;
    }
    AddGarfieldOpenEyeMorph(G, LastMorph);
    if (!G.OK) return G.Error;

    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    bool AddedTail = false;
    for (UEdGraph* AnimationGraph : Graphs)
    {
        UAnimGraphNode_ModifyBone* RootScale = nullptr;
        for (UEdGraphNode* Node : AnimationGraph->Nodes)
            if (auto* Bone = Cast<UAnimGraphNode_ModifyBone>(Node))
                if (Bone->Node.BoneToModify.BoneName == TEXT("Placer")) RootScale = Bone;
        if (!RootScale) continue;
        Graph A{Blueprint, AnimationGraph};
        auto* Input = A.P(RootScale, TEXT("ComponentPose"));
        if (!Input || Input->LinkedTo.Num() != 1) return TEXT("unexpected scale input");
        auto* Current = Input->LinkedTo[0];
        Input->BreakAllPinLinks();
        for (int32 Index = 0; Index < 3; ++Index)
        {
            auto* Rate = A.Call(UKismetMathLibrary::StaticClass(), TEXT("Multiply_DoubleDouble"));
            A.Link(A.Get(TEXT("LifeClock")), A.P(Rate, TEXT("A")));
            A.Lit(Rate, TEXT("B"), TEXT("1.8"));
            auto* Phase = A.Call(UKismetMathLibrary::StaticClass(), TEXT("Add_DoubleDouble"));
            A.Link(A.Ret(Rate), A.P(Phase, TEXT("A")));
            A.Lit(Phase, TEXT("B"), FString::SanitizeFloat(-.6 * Index));
            auto* Wave = A.Call(UKismetMathLibrary::StaticClass(), TEXT("Sin"));
            A.Link(A.Ret(Phase), A.P(Wave, TEXT("A")));
            auto* Amplitude = A.Call(UKismetMathLibrary::StaticClass(), TEXT("Multiply_DoubleDouble"));
            A.Link(A.Ret(Wave), A.P(Amplitude, TEXT("A")));
            A.Lit(Amplitude, TEXT("B"), FString::SanitizeFloat(8 + 4 * Index));
            auto* Rotation = A.Call(UKismetMathLibrary::StaticClass(), TEXT("MakeRotator"));
            A.Link(A.Ret(Amplitude), A.P(Rotation, TEXT("Yaw")));
            FGraphNodeCreator<UAnimGraphNode_ModifyBone> Creator(*AnimationGraph);
            auto* Bone = Creator.CreateNode();
            Bone->Node.BoneToModify.BoneName = FName(*FString::Printf(TEXT("tail%d"), Index * 2 + 1));
            Bone->Node.RotationMode = BMM_Additive;
            Bone->Node.RotationSpace = BCS_BoneSpace;
            Bone->Node.Alpha = 1;
            Creator.Finalize();
            for (int32 Pin = 0; Pin < Bone->ShowPinForProperties.Num(); ++Pin)
                if (Bone->ShowPinForProperties[Pin].PropertyName == TEXT("Rotation")) Bone->SetPinVisibility(true, Pin);
            A.Link(A.Ret(Rotation), A.P(Bone, TEXT("Rotation")));
            A.Link(Current, A.P(Bone, TEXT("ComponentPose")));
            for (auto* Pin : Bone->Pins) if (Pin->Direction == EGPD_Output) Current = Pin;
        }
        A.Link(Current, Input);
        if (!A.OK) return A.Error;
        AddedTail = true;
        break;
    }
    if (!AddedTail) return TEXT("root scale node missing");
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    Blueprint->MarkPackageDirty();
    return Blueprint->Status == BS_Error ? TEXT("liveliness compile failed") : TEXT("OK");
}

#include "Components/SizeBox.h"

FString UHcxRetargetTools::PolishGarfieldScalePanel()
{
    using namespace HcxPanel;
    auto* Widget = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/Garfield/WBP_GarfieldScale"));
    if (!Widget) return TEXT("scale panel missing");
    UWidgetTree* Tree = Widget->WidgetTree;
    auto* Border = Cast<UBorder>(Tree->FindWidget(TEXT("ControlsPanel")));
    auto* Slot = Border ? Cast<UCanvasPanelSlot>(Border->Slot) : nullptr;
    if (!Slot) return TEXT("scale panel layout missing");
    if (!Cast<USizeBox>(Border->GetContent()))
    {
        auto* Content = Border->GetContent();
        Border->SetContent(nullptr);
        auto* Width = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PanelWidth"));
        Width->SetWidthOverride(440);
        Width->SetContent(Content);
        Border->SetContent(Width);
    }
    Slot->SetAutoSize(true);
    auto Label = [&](FName Name, const FString& Text)
    {
        auto* Block = Cast<UTextBlock>(Tree->FindWidget(Name));
        if (Block) { Block->SetText(FText::FromString(Text)); Block->SetAutoWrapText(true); }
    };
    Label(TEXT("Title"), TEXT("加菲猫体型调节  /  F8"));
    Label(TEXT("Caption"), TEXT("体型百分比（实时预览）"));
    Label(TEXT("Status"), TEXT("按 F8 关闭面板。"));
    for (const auto& Entry : {TPair<FName, FString>(TEXT("ResetButton"), TEXT("恢复 100%")),
                             TPair<FName, FString>(TEXT("SaveButton"), TEXT("保存设置"))})
    {
        auto* Button = Cast<UButton>(Tree->FindWidget(Entry.Key));
        auto* Text = Button ? Cast<UTextBlock>(Button->GetContent()) : nullptr;
        if (!Text) return TEXT("scale button missing");
        Text->SetText(FText::FromString(Entry.Value));
        auto Font = Text->GetFont(); Font.Size = 18; Text->SetFont(Font);
        if (auto* ButtonSlot = Cast<UHorizontalBoxSlot>(Button->Slot))
        {
            ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            ButtonSlot->SetPadding(FMargin(4, 0));
        }
    }
    for (UEdGraph* Graph : Widget->UbergraphPages)
        for (UEdGraphNode* Node : Graph->Nodes)
            if (auto* Function = Cast<UK2Node_CallFunction>(Node))
                if (Function->FunctionReference.GetMemberName() == TEXT("Conv_StringToText"))
                    if (auto* Text = Function->FindPin(TEXT("InString")))
                    {
                        if (Text->DefaultValue.StartsWith(TEXT("Saved /")) || Text->DefaultValue.StartsWith(TEXT("已保存"))) Text->DefaultValue = TEXT("已保存。");
                        if (Text->DefaultValue.StartsWith(TEXT("Save failed /")) || Text->DefaultValue.StartsWith(TEXT("保存失败"))) Text->DefaultValue = TEXT("保存失败，请重试。");
                    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Widget);
    FKismetEditorUtilities::CompileBlueprint(Widget);
    Widget->MarkPackageDirty();
    return Widget->Status == BS_Error ? TEXT("Chinese panel compile failed") : TEXT("OK");
}
