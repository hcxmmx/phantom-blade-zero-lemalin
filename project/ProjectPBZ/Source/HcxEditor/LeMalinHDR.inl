#include "Components/PostProcessComponent.h"

static void AddLeMalinHDRProperties(UBlueprint* BP)
{
 FEdGraphPinType Object;Object.PinCategory=UEdGraphSchema_K2::PC_Object;
 FEdGraphPinType Bool;Bool.PinCategory=UEdGraphSchema_K2::PC_Boolean;
 FEdGraphPinType Int;Int.PinCategory=UEdGraphSchema_K2::PC_Int;
 auto Add=[&](FName N,FEdGraphPinType T,bool Array=false,const TCHAR* Default=TEXT("")){if(Array)T.ContainerType=EPinContainerType::Array;FBlueprintEditorUtils::AddMemberVariable(BP,N,T,Default);};
 Object.PinSubCategoryObject=UMaterialInstanceDynamic::StaticClass();Add(TEXT("HDRMID"),Object);
 Object.PinSubCategoryObject=UPostProcessComponent::StaticClass();Add(TEXT("HDRVolume"),Object);
 Object.PinSubCategoryObject=UPrimitiveComponent::StaticClass();Add(TEXT("HDRTargets"),Object,true);
 Add(TEXT("HDRSavedDepth"),Bool,true);Add(TEXT("HDRSavedStencil"),Int,true);Add(TEXT("HDRRestoreIndex"),Int);Add(TEXT("HDRFound"),Int);Add(TEXT("HDRPrevDepthMode"),Int,false,TEXT("-1"));Add(TEXT("HDRLoadTried"),Bool);
 FEdGraphPinType Mask;Mask.PinCategory=UEdGraphSchema_K2::PC_Byte;Mask.PinSubCategoryObject=StaticEnum<ERendererStencilMask>();Add(TEXT("HDRSavedMask"),Mask,true);
 FEdGraphPinType Soft;Soft.PinCategory=UEdGraphSchema_K2::PC_SoftObject;Soft.PinSubCategoryObject=UMaterialInterface::StaticClass();Add(TEXT("HDRMaterialAsset"),Soft,false,TEXT("/Game/LeMalin/Materials/M_LeMalinHDR_Prototype.M_LeMalinHDR_Prototype"));
}

struct FLeMalinHDRGraph
{
 HcxPanel::Graph& G;UEdGraph* E;UEdGraphPin* Own;UClass* WidgetClass;
 UEdGraphPin* Widget(){return G.Cast(G.Get(TEXT("LeMalinPanel")),WidgetClass);}
 UEdGraphPin* Get(FName N){return G.Get(N,WidgetClass,Widget());}
 UEdGraphPin* Op(FName F,UEdGraphPin* A,UEdGraphPin* B){auto* N=G.Call(UKismetMathLibrary::StaticClass(),F);G.Link(A,G.P(N,TEXT("A")));G.Link(B,G.P(N,TEXT("B")));return G.Ret(N);}
 UEdGraphPin* Set(UEdGraphPin* X,FName N,UEdGraphPin* Value){auto* S=N.ToString().StartsWith(TEXT("HDR"))?G.Set(N,WidgetClass,Widget()):G.Set(N);G.Link(Value,G.P(S,N));return G.Run(X,S);}
 TPair<UEdGraphPin*,UEdGraphPin*> Join(){FGraphNodeCreator<UK2Node_ExecutionSequence> C(*E);auto* N=C.CreateNode();C.Finalize();return {G.P(N,UEdGraphSchema_K2::PN_Execute),N->GetThenPinGivenIndex(0)};}
 UK2Node_CallArrayFunction* Array(FName F,FName Member){FGraphNodeCreator<UK2Node_CallArrayFunction> C(*E);auto* N=C.CreateNode();N->SetFromFunction(UKismetArrayLibrary::StaticClass()->FindFunctionByName(F));C.Finalize();G.Link(Get(Member),G.P(N,TEXT("TargetArray")));return N;}
 UEdGraphPin* Item(FName Member,UEdGraphPin* Index){FGraphNodeCreator<UK2Node_GetArrayItem> C(*E);auto* N=C.CreateNode();C.Finalize();G.Link(Get(Member),N->GetTargetArrayPin());G.Link(Index,N->GetIndexPin());return N->GetResultPin();}
 UEdGraphPin* Append(UEdGraphPin* X,FName Member,UEdGraphPin* Value){auto* N=Array(TEXT("Array_Add"),Member);G.Link(Value,G.P(N,TEXT("NewItem")));return G.Run(X,N);}
 UEdGraphPin* Active(){return Op(TEXT("BooleanAND"),Op(TEXT("Greater_DoubleDouble"),G.Get(TEXT("LeMalinHDRMode")),G.Number(.5)),Op(TEXT("BooleanAND"),G.Valid(Get(TEXT("HDRMID"))),G.Valid(Get(TEXT("HDRVolume")))));}
 UEdGraphPin* VolumeValue(UEdGraphPin* X,FName Name,const TCHAR* Value){auto* N=G.Set(Name,UPostProcessComponent::StaticClass(),Get(TEXT("HDRVolume")));G.Lit(N,Name,Value);return G.Run(X,N);}
 UK2Node_CallFunction* Primitive(FName F,UEdGraphPin* Target){auto* N=G.Call(UPrimitiveComponent::StaticClass(),F);G.Link(Target,G.P(N,UEdGraphSchema_K2::PN_Self));return N;}
 UEdGraphPin* Console(UEdGraphPin* X,UEdGraphPin* Command){auto* N=G.Call(UKismetSystemLibrary::StaticClass(),TEXT("ExecuteConsoleCommand"));G.Link(Own,G.P(N,TEXT("WorldContextObject")));G.Link(Command,G.P(N,TEXT("Command")));return G.Run(X,N);}
 UK2Node_CallFunction* DepthMode(){auto* N=G.Call(UKismetSystemLibrary::StaticClass(),TEXT("GetConsoleVariableIntValue"));G.Lit(N,TEXT("VariableName"),TEXT("r.CustomDepth"));return N;}
 UEdGraphPin* Setup(UEdGraphPin* In)
 {
  auto Finish=Join();auto* Mode=G.Branch(In,Op(TEXT("Greater_DoubleDouble"),G.Get(TEXT("LeMalinHDRMode")),G.Number(.5)));
  // Disable is immediate and restores the component flags/stencil values we changed.
  auto* HasVolume=G.Branch(G.P(Mode,UEdGraphSchema_K2::PN_Else),G.Valid(Get(TEXT("HDRVolume"))));auto OffReady=Join();G.Link(G.P(HasVolume,UEdGraphSchema_K2::PN_Else),OffReady.Key);
  auto* X=VolumeValue(G.P(HasVolume,UEdGraphSchema_K2::PN_Then),TEXT("BlendWeight"),TEXT("0"));X=VolumeValue(X,TEXT("bEnabled"),TEXT("false"));G.Link(X,OffReady.Key);
  auto* Index=Get(TEXT("HDRRestoreIndex"));auto* Count=Array(TEXT("Array_Length"),TEXT("HDRTargets"));auto* Loop=G.Branch(Set(OffReady.Value,TEXT("HDRRestoreIndex"),G.Number(0)),Op(TEXT("Less_IntInt"),Index,G.Ret(Count)));auto Next=Join();auto* Target=Item(TEXT("HDRTargets"),Index);auto* Valid=G.Branch(G.P(Loop,UEdGraphSchema_K2::PN_Then),G.Valid(Target));G.Link(G.P(Valid,UEdGraphSchema_K2::PN_Else),Next.Key);
  auto* Depth=Primitive(TEXT("SetRenderCustomDepth"),Target);G.Link(Item(TEXT("HDRSavedDepth"),Index),G.P(Depth,TEXT("bValue")));X=G.Run(G.P(Valid,UEdGraphSchema_K2::PN_Then),Depth);
  auto* Stencil=Primitive(TEXT("SetCustomDepthStencilValue"),Target);G.Link(Item(TEXT("HDRSavedStencil"),Index),G.P(Stencil,TEXT("Value")));X=G.Run(X,Stencil);
  auto* Mask=Primitive(TEXT("SetCustomDepthStencilWriteMask"),Target);G.Link(Item(TEXT("HDRSavedMask"),Index),G.P(Mask,TEXT("WriteMaskBit")));X=G.Run(X,Mask);G.Link(X,Next.Key);G.Link(Set(Next.Value,TEXT("HDRRestoreIndex"),Op(TEXT("Add_IntInt"),Index,G.Number(1))),G.P(Loop,UEdGraphSchema_K2::PN_Execute));
  auto* ResetTry=G.Set(TEXT("HDRLoadTried"),WidgetClass,Widget());G.Lit(ResetTry,TEXT("HDRLoadTried"),TEXT("false"));X=G.Run(G.P(Loop,UEdGraphSchema_K2::PN_Else),ResetTry);for(FName N:{TEXT("HDRTargets"),TEXT("HDRSavedDepth"),TEXT("HDRSavedStencil"),TEXT("HDRSavedMask")})X=G.Run(X,Array(TEXT("Array_Clear"),N));
  auto* RestoreCvar=G.Branch(X,Op(TEXT("GreaterEqual_IntInt"),Get(TEXT("HDRPrevDepthMode")),G.Number(0)));G.Link(G.P(RestoreCvar,UEdGraphSchema_K2::PN_Else),Finish.Key);
  auto* ToString=G.Call(UKismetStringLibrary::StaticClass(),TEXT("Conv_IntToString"));G.Link(Get(TEXT("HDRPrevDepthMode")),G.P(ToString,TEXT("InInt")));auto* Command=G.Call(UKismetStringLibrary::StaticClass(),TEXT("Concat_StrStr"));G.Lit(Command,TEXT("A"),TEXT("r.CustomDepth "));G.Link(G.Ret(ToString),G.P(Command,TEXT("B")));X=Console(G.P(RestoreCvar,UEdGraphSchema_K2::PN_Then),G.Ret(Command));G.Link(Set(X,TEXT("HDRPrevDepthMode"),G.Number(-1)),Finish.Key);
  // Soft-load only after the user enables the prototype; never persist enablement.
  auto* Have=G.Branch(G.P(Mode,UEdGraphSchema_K2::PN_Then),Op(TEXT("BooleanAND"),G.Valid(Get(TEXT("HDRMID"))),G.Valid(Get(TEXT("HDRVolume")))));auto Ready=Join();auto Failed=Join();G.Link(G.P(Have,UEdGraphSchema_K2::PN_Then),Ready.Key);
  auto* Tried=G.Branch(G.P(Have,UEdGraphSchema_K2::PN_Else),Get(TEXT("HDRLoadTried")));G.Link(G.P(Tried,UEdGraphSchema_K2::PN_Then),Failed.Key);auto* Mark=G.Set(TEXT("HDRLoadTried"),WidgetClass,Widget());G.Lit(Mark,TEXT("HDRLoadTried"),TEXT("true"));X=G.Run(G.P(Tried,UEdGraphSchema_K2::PN_Else),Mark);
  auto* Load=G.Call(UKismetSystemLibrary::StaticClass(),TEXT("LoadAsset_Blocking"));G.Link(Get(TEXT("HDRMaterialAsset")),G.P(Load,TEXT("Asset")));X=G.Run(X,Load);auto* Mat=G.Cast(G.Ret(Load),UMaterialInterface::StaticClass());auto* Loaded=G.Branch(X,G.Valid(Mat));G.Link(G.P(Loaded,UEdGraphSchema_K2::PN_Else),Failed.Key);
  auto* Owner=G.Call(UActorComponent::StaticClass(),TEXT("GetOwner"));G.Link(Own,G.P(Owner,UEdGraphSchema_K2::PN_Self));auto* Add=G.Call(AActor::StaticClass(),TEXT("AddComponentByClass"));G.Link(G.Ret(Owner),G.P(Add,UEdGraphSchema_K2::PN_Self));G.Obj(Add,TEXT("Class"),UPostProcessComponent::StaticClass());G.Lit(Add,TEXT("bManualAttachment"),TEXT("true"));G.Lit(Add,TEXT("bDeferredFinish"),TEXT("false"));auto* Transform=G.Call(UKismetMathLibrary::StaticClass(),TEXT("MakeTransform"));G.Link(G.Ret(Transform),G.P(Add,TEXT("RelativeTransform")));X=G.Run(G.P(Loaded,UEdGraphSchema_K2::PN_Then),Add);X=Set(X,TEXT("HDRVolume"),G.Cast(G.Ret(Add),UPostProcessComponent::StaticClass()));
  auto* Created=G.Branch(X,G.Valid(Get(TEXT("HDRVolume"))));G.Link(G.P(Created,UEdGraphSchema_K2::PN_Else),Failed.Key);X=VolumeValue(G.P(Created,UEdGraphSchema_K2::PN_Then),TEXT("BlendWeight"),TEXT("0"));X=VolumeValue(X,TEXT("bEnabled"),TEXT("false"));X=VolumeValue(X,TEXT("bUnbound"),TEXT("true"));X=VolumeValue(X,TEXT("Priority"),TEXT("1000"));
  auto* CreateMID=G.Call(UKismetMaterialLibrary::StaticClass(),TEXT("CreateDynamicMaterialInstance"));G.Link(Own,G.P(CreateMID,TEXT("WorldContextObject")));G.Link(Mat,G.P(CreateMID,TEXT("Parent")));X=G.Run(X,CreateMID);X=Set(X,TEXT("HDRMID"),G.Ret(CreateMID));auto* HasMID=G.Branch(X,G.Valid(Get(TEXT("HDRMID"))));G.Link(G.P(HasMID,UEdGraphSchema_K2::PN_Else),Failed.Key);
  auto* Blend=G.Call(UPostProcessComponent::StaticClass(),TEXT("AddOrUpdateBlendable"));G.Link(Get(TEXT("HDRVolume")),G.P(Blend,UEdGraphSchema_K2::PN_Self));G.Link(Get(TEXT("HDRMID")),G.P(Blend,TEXT("InBlendableObject")));G.Lit(Blend,TEXT("InWeight"),TEXT("1"));G.Link(G.Run(G.P(HasMID,UEdGraphSchema_K2::PN_Then),Blend),Ready.Key);
  auto* Backup=G.Branch(Ready.Value,Op(TEXT("Less_IntInt"),Get(TEXT("HDRPrevDepthMode")),G.Number(0)));auto Backed=Join();G.Link(G.P(Backup,UEdGraphSchema_K2::PN_Else),Backed.Key);auto* OldMode=DepthMode();X=G.Run(G.P(Backup,UEdGraphSchema_K2::PN_Then),OldMode);G.Link(Set(X,TEXT("HDRPrevDepthMode"),G.Ret(OldMode)),Backed.Key);
  auto* Current=DepthMode();X=G.Run(Backed.Value,Current);auto* NeedDepth=G.Branch(X,Op(TEXT("EqualEqual_IntInt"),G.Ret(Current),G.Number(3)));auto DepthReady=Join();G.Link(G.P(NeedDepth,UEdGraphSchema_K2::PN_Then),DepthReady.Key);
  auto* ExecCommand=G.Call(UKismetSystemLibrary::StaticClass(),TEXT("ExecuteConsoleCommand"));G.Link(Own,G.P(ExecCommand,TEXT("WorldContextObject")));G.Lit(ExecCommand,TEXT("Command"),TEXT("r.CustomDepth 3"));G.Link(G.Run(G.P(NeedDepth,UEdGraphSchema_K2::PN_Else),ExecCommand),DepthReady.Key);
  auto* Check=DepthMode();X=G.Run(DepthReady.Value,Check);auto* Supported=G.Branch(X,Op(TEXT("EqualEqual_IntInt"),G.Ret(Check),G.Number(3)));G.Link(G.P(Supported,UEdGraphSchema_K2::PN_Else),Failed.Key);
  auto* SetMode=G.Call(UMaterialInstanceDynamic::StaticClass(),TEXT("SetScalarParameterValue"));G.Link(Get(TEXT("HDRMID")),G.P(SetMode,UEdGraphSchema_K2::PN_Self));G.Lit(SetMode,TEXT("ParameterName"),TEXT("Mode"));G.Link(G.Get(TEXT("LeMalinHDRMode")),G.P(SetMode,TEXT("Value")));X=G.Run(G.P(Supported,UEdGraphSchema_K2::PN_Then),SetMode);X=VolumeValue(X,TEXT("bEnabled"),TEXT("true"));X=VolumeValue(X,TEXT("BlendWeight"),TEXT("1"));G.Link(X,Finish.Key);
  X=Set(Failed.Value,TEXT("LeMalinHDRMode"),G.Number(0));auto* Widget=G.Cast(G.Get(TEXT("LeMalinPanel")),WidgetClass);auto* Slider=G.Call(USlider::StaticClass(),TEXT("SetValue"));G.Link(G.Get(TEXT("HDRModeSlider"),WidgetClass,Widget),G.P(Slider,UEdGraphSchema_K2::PN_Self));G.Lit(Slider,TEXT("InValue"),TEXT("0"));X=G.Run(X,Slider);X=G.Text(X,TEXT("HDRModeValue"),G.Number(0),WidgetClass,Widget);
  auto* Message=G.Call(UKismetTextLibrary::StaticClass(),TEXT("Conv_StringToText"));G.Lit(Message,TEXT("InString"),TEXT("受光原型未能启用，已关闭；可使用底色 3 档。"));auto* Text=G.Call(UTextBlock::StaticClass(),TEXT("SetText"));G.Link(G.Get(TEXT("Status"),WidgetClass,Widget),G.P(Text,UEdGraphSchema_K2::PN_Self));G.Link(G.Ret(Message),G.P(Text,TEXT("InText")));G.Link(G.Run(X,Text),Finish.Key);
  return Finish.Value;
 }
 UEdGraphPin* TrackTarget(UEdGraphPin* In,UEdGraphPin* Target)
 {
  auto End=Join();auto* Enable=G.Branch(In,Active());G.Link(G.P(Enable,UEdGraphSchema_K2::PN_Else),End.Key);
  auto* Find=Array(TEXT("Array_Find"),TEXT("HDRTargets"));G.Link(Target,G.P(Find,TEXT("ItemToFind")));auto* X=Set(G.P(Enable,UEdGraphSchema_K2::PN_Then),TEXT("HDRFound"),G.Ret(Find));auto* Cached=G.Branch(X,Op(TEXT("GreaterEqual_IntInt"),Get(TEXT("HDRFound")),G.Number(0)));auto Ready=Join();G.Link(G.P(Cached,UEdGraphSchema_K2::PN_Then),Ready.Key);
  X=Append(G.P(Cached,UEdGraphSchema_K2::PN_Else),TEXT("HDRTargets"),Target);X=Append(X,TEXT("HDRSavedDepth"),G.Get(TEXT("bRenderCustomDepth"),UPrimitiveComponent::StaticClass(),Target));X=Append(X,TEXT("HDRSavedStencil"),G.Get(TEXT("CustomDepthStencilValue"),UPrimitiveComponent::StaticClass(),Target));X=Append(X,TEXT("HDRSavedMask"),G.Get(TEXT("CustomDepthStencilWriteMask"),UPrimitiveComponent::StaticClass(),Target));G.Link(X,Ready.Key);
  auto* Mask=Primitive(TEXT("SetCustomDepthStencilWriteMask"),Target);G.Lit(Mask,TEXT("WriteMaskBit"),TEXT("ERSM_255"));X=G.Run(Ready.Value,Mask);auto* Stencil=Primitive(TEXT("SetCustomDepthStencilValue"),Target);G.Lit(Stencil,TEXT("Value"),TEXT("231"));X=G.Run(X,Stencil);auto* Depth=Primitive(TEXT("SetRenderCustomDepth"),Target);G.Lit(Depth,TEXT("bValue"),TEXT("true"));G.Link(G.Run(X,Depth),End.Key);return End.Value;
 }
};
