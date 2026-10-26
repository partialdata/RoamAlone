#include "VanPromptWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

void UVanPromptWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;
	Prompt = WidgetTree->ConstructWidget<UTextBlock>();
	Prompt->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFontStyle("Regular", 19)));
	Prompt->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Prompt->SetShadowColorAndOpacity(FLinearColor::Black);
	Prompt->SetShadowOffset(FVector2D(1, 1));
	UCanvasPanelSlot* PromptSlot = Canvas->AddChildToCanvas(Prompt);
	PromptSlot->SetPosition(FVector2D(28, 28));
	PromptSlot->SetAutoSize(true);

	UTextBlock* Reticle = WidgetTree->ConstructWidget<UTextBlock>();
	Reticle->SetText(FText::FromString(TEXT("+")));
	UCanvasPanelSlot* ReticleSlot = Canvas->AddChildToCanvas(Reticle);
	ReticleSlot->SetAnchors(FAnchors(0.5, 0.5));
	ReticleSlot->SetAlignment(FVector2D(0.5, 0.5));
	ReticleSlot->SetAutoSize(true);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UVanPromptWidget::SetPrompt(const FText& Text)
{
	if (Prompt && !Prompt->GetText().EqualTo(Text))
	{
		Prompt->SetText(Text);
	}
}
