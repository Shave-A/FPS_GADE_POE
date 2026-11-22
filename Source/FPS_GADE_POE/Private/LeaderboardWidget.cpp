#include "LeaderboardWidget.h"
#include "MatchManager.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"

void ULeaderboardWidget::NativeConstruct()
{
    Super::NativeConstruct();
    Refresh();

    // Observer: redraw whenever someone scores (keeps the pause menu live)
    if (UMatchManager* Manager = GetWorld()->GetSubsystem<UMatchManager>())
    {
        ScoresHandle = Manager->OnScoresChanged.AddUObject(this, &ULeaderboardWidget::Refresh);
    }
}

void ULeaderboardWidget::NativeDestruct()
{
    if (GetWorld())
    {
        if (UMatchManager* Manager = GetWorld()->GetSubsystem<UMatchManager>())
        {
            Manager->OnScoresChanged.Remove(ScoresHandle);
        }
    }
    Super::NativeDestruct();
}

void ULeaderboardWidget::Refresh()
{
    UMatchManager* Manager = GetWorld() ? GetWorld()->GetSubsystem<UMatchManager>() : nullptr;
    if (!Manager || !EntriesBox) return;

    EntriesBox->ClearChildren();

    int32 Rank = 1;
    for (const FLeaderboardEntry& Entry : Manager->GetLeaderboard())
    {
        UBorder* Row = WidgetTree->ConstructWidget<UBorder>();
        Row->SetBrushColor(Entry.bIsPlayer ? PlayerRowColor : RowColor);   // player's row is highlighted
        Row->SetPadding(FMargin(12.f, 6.f));

        UHorizontalBox* Cells = WidgetTree->ConstructWidget<UHorizontalBox>();
        Row->SetContent(Cells);

        auto AddCell = [&](const FString& Text, float Weight, ETextJustify::Type Justify)
            {
                UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
                Label->SetText(FText::FromString(Text));
                Label->SetJustification(Justify);
                FSlateFontInfo Font = Label->GetFont();
                Font.Size = FontSize;
                Label->SetFont(Font);

                UHorizontalBoxSlot* Slot = Cells->AddChildToHorizontalBox(Label);
                FSlateChildSize Size;
                Size.SizeRule = ESlateSizeRule::Fill;
                Size.Value = Weight;
                Slot->SetSize(Size);
                Slot->SetPadding(FMargin(8.f, 0.f));
            };

        AddCell(FString::Printf(TEXT("%d."), Rank), 0.6f, ETextJustify::Left);
        AddCell(Entry.bIsPlayer ? Entry.Name + TEXT("  (YOU)") : Entry.Name, 3.f, ETextJustify::Left);
        AddCell(FString::Printf(TEXT("%d kills"), Entry.Kills), 1.5f, ETextJustify::Right);

        EntriesBox->AddChildToVerticalBox(Row);
        ++Rank;
    }
}