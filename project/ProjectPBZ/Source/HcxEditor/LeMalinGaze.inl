// Editor-generated gaze curves share the existing morph/eyelid runtime path.
static UEdGraphPin* BuildLeMalinGazeGraph(HcxPanel::Graph& G,UK2Node_Event* Update,UEdGraphPin* Exec)
{
 auto Math=[&](FName Function,UEdGraphPin* A,UEdGraphPin* B){
  auto* Node=G.Call(UKismetMathLibrary::StaticClass(),Function);
  G.Link(A,G.P(Node,TEXT("A")));G.Link(B,G.P(Node,TEXT("B")));return G.Ret(Node);
 };
 auto Select=[&](UEdGraphPin* Condition,UEdGraphPin* A,UEdGraphPin* B){
  auto* Node=G.Call(UKismetMathLibrary::StaticClass(),TEXT("SelectFloat"));
  G.Link(Condition,G.P(Node,TEXT("bPickA")));G.Link(A,G.P(Node,TEXT("A")));G.Link(B,G.P(Node,TEXT("B")));return G.Ret(Node);
 };
 auto* Tick=G.Set(TEXT("GazeClock"));
 G.Link(Math(TEXT("Add_DoubleDouble"),G.Get(TEXT("GazeClock")),G.P(Update,TEXT("DeltaTimeX"))),G.P(Tick,TEXT("GazeClock")));Exec=G.Run(Exec,Tick);
 auto* BlinkOpportunity=Math(TEXT("BooleanAND"),Math(TEXT("Greater_DoubleDouble"),G.Get(TEXT("BlinkProgress")),G.Number(.7)),Math(TEXT("Greater_DoubleDouble"),G.Get(TEXT("GazeClock")),G.Number(-1)));
 auto* Reset=G.Branch(Exec,Math(TEXT("BooleanOR"),Math(TEXT("GreaterEqual_DoubleDouble"),G.Get(TEXT("GazeClock")),G.Number(0)),BlinkOpportunity));
 auto* Delay=G.Call(UKismetMathLibrary::StaticClass(),TEXT("RandomFloatInRange"));G.Lit(Delay,TEXT("Min"),TEXT("2"));G.Lit(Delay,TEXT("Max"),TEXT("5"));
 auto* StoreClock=G.Set(TEXT("GazeClock"));G.Link(Math(TEXT("Multiply_DoubleDouble"),G.Ret(Delay),G.Number(-1)),G.P(StoreClock,TEXT("GazeClock")));
 auto* Changed=G.Run(G.P(Reset,UEdGraphSchema_K2::PN_Then),StoreClock);
 for(const TCHAR* Axis:{TEXT("H"),TEXT("V")}){
  FName Name(*(FString(TEXT("GazeGoal"))+Axis));
  auto* Random=G.Call(UKismetMathLibrary::StaticClass(),TEXT("RandomFloatInRange"));
  G.Lit(Random,TEXT("Min"),FString(Axis)==TEXT("H")?TEXT("-1"):TEXT("-.6"));G.Lit(Random,TEXT("Max"),FString(Axis)==TEXT("H")?TEXT("1"):TEXT(".6"));
  auto* Store=G.Set(Name);G.Link(G.Ret(Random),G.P(Store,Name));Changed=G.Run(Changed,Store);
 }
 // The visible mesh is attached to the original character's mesh component.
 auto* Own=G.Call(UAnimInstance::StaticClass(),TEXT("GetOwningComponent"));
 auto* Parent=G.Call(USceneComponent::StaticClass(),TEXT("GetAttachParent"));G.Link(G.Ret(Own),G.P(Parent,UEdGraphSchema_K2::PN_Self));
 auto* Source=G.Call(UKismetMathLibrary::StaticClass(),TEXT("SelectObject"));
 G.Link(G.Ret(Parent),G.P(Source,TEXT("A")));G.Link(G.Ret(Own),G.P(Source,TEXT("B")));G.Link(G.Valid(G.Ret(Parent)),G.P(Source,TEXT("bSelectA")));
 auto* Owner=G.Call(UActorComponent::StaticClass(),TEXT("GetOwner"));G.Link(G.Cast(G.Ret(Source),UActorComponent::StaticClass()),G.P(Owner,UEdGraphSchema_K2::PN_Self));
 auto* Velocity=G.Call(AActor::StaticClass(),TEXT("GetVelocity"));G.Link(G.Ret(Owner),G.P(Velocity,UEdGraphSchema_K2::PN_Self));
 auto* Speed=G.Call(UKismetMathLibrary::StaticClass(),TEXT("VSize"));G.Link(G.Ret(Velocity),G.P(Speed,TEXT("A")));
 auto* Moving=Math(TEXT("Greater_DoubleDouble"),G.Ret(Speed),G.Number(10));
 auto IsMode=[&](int32 Value){return Math(TEXT("NearlyEqual_FloatFloat"),G.Get(TEXT("LeMalinGazeMode")),G.Number(Value));};
 auto* Automatic=IsMode(1);
 for(int32 Axis=0;Axis<2;++Axis){
  FName CurrentName(Axis==0?TEXT("GazeCurrentH"):TEXT("GazeCurrentV"));
  auto* Goal=G.Get(Axis==0?TEXT("GazeGoalH"):TEXT("GazeGoalV"));
  auto* Fixed=Select(IsMode(Axis==0?2:4),G.Number(1),Select(IsMode(Axis==0?3:5),G.Number(-1),G.Number(0)));
  auto* Target=Select(Automatic,Select(Moving,G.Number(0),Goal),Fixed);
  auto* Interp=G.Call(UKismetMathLibrary::StaticClass(),TEXT("FInterpTo"));
  G.Link(G.Get(CurrentName),G.P(Interp,TEXT("Current")));G.Link(Target,G.P(Interp,TEXT("Target")));
  G.Link(G.P(Update,TEXT("DeltaTimeX")),G.P(Interp,TEXT("DeltaTime")));G.Lit(Interp,TEXT("InterpSpeed"),TEXT("12"));
  auto* Store=G.Set(CurrentName);G.Link(G.Ret(Interp),G.P(Store,CurrentName));
  if(Axis==0){Exec=G.Run(Changed,Store);G.Link(G.P(Reset,UEdGraphSchema_K2::PN_Else),G.P(Store,UEdGraphSchema_K2::PN_Execute));}
  else Exec=G.Run(Exec,Store);
 }
 const TCHAR* Names[]={TEXT("GazeLeft"),TEXT("GazeRight"),TEXT("GazeUp"),TEXT("GazeDown")};
 for(int32 Index=0;Index<4;++Index){
  auto* Signed=Math(TEXT("Multiply_DoubleDouble"),G.Get(Index<2?TEXT("GazeCurrentH"):TEXT("GazeCurrentV")),G.Number(Index%2==0?1:-1));
  auto* Weight=Math(TEXT("Multiply_DoubleDouble"),Math(TEXT("FMax"),Signed,G.Number(0)),G.Get(TEXT("LeMalinGazeStrength")));
  Weight=Math(TEXT("Multiply_DoubleDouble"),Weight,Math(TEXT("Subtract_DoubleDouble"),G.Number(1),G.Get(TEXT("BlinkProgress"))));
  auto* Apply=G.Call(UAnimInstance::StaticClass(),TEXT("SetMorphTarget"));G.Lit(Apply,TEXT("MorphTargetName"),Names[Index]);G.Link(Weight,G.P(Apply,TEXT("Value")));Exec=G.Run(Exec,Apply);
 }
 return Exec;
}
