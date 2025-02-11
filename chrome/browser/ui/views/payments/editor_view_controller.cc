// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/editor_view_controller.h"

#include <map>
#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/browser/ui/views/payments/validating_combobox.h"
#include "chrome/browser/ui/views/payments/validating_textfield.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

namespace payments {
namespace {

constexpr int kFirstTagValue = static_cast<int>(
    payments::PaymentRequestCommonTags::PAYMENT_REQUEST_COMMON_TAG_MAX);

enum class EditorViewControllerTags : int {
  // The tag for the button that saves the model being edited. Starts at
  // |kFirstTagValue| not to conflict with tags common to all views.
  SAVE_BUTTON = kFirstTagValue,
};

std::unique_ptr<views::View> CreateErrorLabelView(const base::string16& error,
                                                  const EditorField& field) {
  std::unique_ptr<views::View> view = base::MakeUnique<views::View>();

  std::unique_ptr<views::BoxLayout> layout =
      base::MakeUnique<views::BoxLayout>(views::BoxLayout::kVertical, 0, 0, 0);
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_STRETCH);
  // This is the space between the input field and the error label.
  constexpr int kErrorLabelTopPadding = 6;
  layout->set_inside_border_insets(gfx::Insets(kErrorLabelTopPadding, 0, 0, 0));
  view->SetLayoutManager(layout.release());

  std::unique_ptr<views::Label> error_label =
      base::MakeUnique<views::Label>(error);
  error_label->set_id(static_cast<int>(DialogViewID::ERROR_LABEL_OFFSET) +
                      field.type);
  error_label->SetFontList(
      error_label->GetDefaultFontList().DeriveWithSizeDelta(-1));
  error_label->SetEnabledColor(error_label->GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_AlertSeverityHigh));

  view->AddChildView(error_label.release());
  return view;
}

}  // namespace

EditorViewController::EditorViewController(
    PaymentRequestSpec* spec,
    PaymentRequestState* state,
    PaymentRequestDialogView* dialog,
    BackNavigationType back_navigation_type)
    : PaymentRequestSheetController(spec, state, dialog),
      first_field_view_(nullptr),
      back_navigation_type_(back_navigation_type) {}

EditorViewController::~EditorViewController() {}

void EditorViewController::DisplayErrorMessageForField(
    const EditorField& field,
    const base::string16& error_message) {
  const auto& label_view_it = error_labels_.find(field);
  DCHECK(label_view_it != error_labels_.end());

  label_view_it->second->RemoveAllChildViews(/*delete_children=*/true);
  if (!error_message.empty()) {
    label_view_it->second->AddChildView(
        CreateErrorLabelView(error_message, field).release());
  }
  RelayoutPane();
}

std::unique_ptr<views::View> EditorViewController::CreateHeaderView() {
  return nullptr;
}

std::unique_ptr<views::View> EditorViewController::CreateCustomFieldView(
    autofill::ServerFieldType type) {
  return nullptr;
}

std::unique_ptr<views::View> EditorViewController::CreateExtraViewForField(
    autofill::ServerFieldType type) {
  return nullptr;
}

std::unique_ptr<views::Button> EditorViewController::CreatePrimaryButton() {
  std::unique_ptr<views::Button> button(
      views::MdTextButton::CreateSecondaryUiBlueButton(
          this, l10n_util::GetStringUTF16(IDS_DONE)));
  button->set_tag(static_cast<int>(EditorViewControllerTags::SAVE_BUTTON));
  button->set_id(static_cast<int>(DialogViewID::EDITOR_SAVE_BUTTON));
  return button;
}

void EditorViewController::FillContentView(views::View* content_view) {
  views::BoxLayout* layout =
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0);
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_STRETCH);
  content_view->SetLayoutManager(layout);
  // No insets. Child views below are responsible for their padding.

  // An editor can optionally have a header view specific to it.
  std::unique_ptr<views::View> header_view = CreateHeaderView();
  if (header_view.get())
    content_view->AddChildView(header_view.release());

  // The heart of the editor dialog: all the input fields with their labels.
  content_view->AddChildView(CreateEditorView().release());
}

void EditorViewController::UpdateEditorView() {
  UpdateContentView();
  // TODO(crbug.com/704254): Find how to update the parent view bounds so that
  // the vertical scrollbar size gets updated.
  dialog()->EditorViewUpdated();
}

void EditorViewController::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  switch (sender->tag()) {
    case static_cast<int>(EditorViewControllerTags::SAVE_BUTTON):
      if (ValidateModelAndSave()) {
        switch (back_navigation_type_) {
          case BackNavigationType::kOneStep:
            dialog()->GoBack();
            break;
          case BackNavigationType::kPaymentSheet:
            dialog()->GoBackToPaymentSheet();
            break;
        }
      }
      break;
    default:
      PaymentRequestSheetController::ButtonPressed(sender, event);
      break;
  }
}

views::View* EditorViewController::GetFirstFocusedView() {
  if (first_field_view_)
    return first_field_view_;
  return PaymentRequestSheetController::GetFirstFocusedView();
}

void EditorViewController::ContentsChanged(views::Textfield* sender,
                                           const base::string16& new_contents) {
  static_cast<ValidatingTextfield*>(sender)->OnContentsChanged();
}

void EditorViewController::OnPerformAction(views::Combobox* sender) {
  static_cast<ValidatingCombobox*>(sender)->OnContentsChanged();
}

std::unique_ptr<views::View> EditorViewController::CreateEditorView() {
  std::unique_ptr<views::View> editor_view = base::MakeUnique<views::View>();
  text_fields_.clear();
  comboboxes_.clear();

  // The editor view is padded horizontally.
  editor_view->SetBorder(views::CreateEmptyBorder(
      0, payments::kPaymentRequestRowHorizontalInsets, 0,
      payments::kPaymentRequestRowHorizontalInsets));

  // All views have fixed size except the Field which stretches. The fixed
  // padding at the end is computed so that Field views have a minimum of
  // 176/272dp (short/long fields) as per spec.
  // ___________________________________________________________________________
  // |Label | 16dp pad | Field (flex) | 8dp pad | Extra View | Computed Padding|
  // |______|__________|______________|_________|____________|_________________|
  constexpr int kLabelWidth = 140;
  // This is the horizontal padding between the label and the field.
  constexpr int kLabelInputFieldHorizontalPadding = 16;
  // This is the horizontal padding between the field and the extra view.
  constexpr int kFieldExtraViewHorizontalPadding = 8;
  constexpr int kShortFieldMinimumWidth = 176;
  constexpr int kLongFieldMinimumWidth = 272;

  std::unique_ptr<views::GridLayout> editor_layout =
      base::MakeUnique<views::GridLayout>(editor_view.get());
  // Column set for short fields.
  views::ColumnSet* columns_short = editor_layout->AddColumnSet(0);
  columns_short->AddColumn(views::GridLayout::LEADING,
                           views::GridLayout::CENTER, 0,
                           views::GridLayout::FIXED, kLabelWidth, 0);
  columns_short->AddPaddingColumn(0, kLabelInputFieldHorizontalPadding);
  // The field view column stretches.
  columns_short->AddColumn(views::GridLayout::LEADING,
                           views::GridLayout::CENTER, 1,
                           views::GridLayout::USE_PREF, 0, 0);
  columns_short->AddPaddingColumn(0, kFieldExtraViewHorizontalPadding);
  // The extra field view column is fixed size, computed from the largest
  // extra view.
  int short_extra_view_width =
      ComputeWidestExtraViewWidth(EditorField::LengthHint::HINT_SHORT);
  columns_short->AddColumn(views::GridLayout::LEADING,
                           views::GridLayout::CENTER, 0,
                           views::GridLayout::FIXED, short_extra_view_width, 0);
  // The padding at the end is fixed, computed to make sure the short field
  // maintains its minimum width.
  int short_padding = kDialogMinWidth - kShortFieldMinimumWidth - kLabelWidth -
                      (2 * kPaymentRequestRowHorizontalInsets) -
                      kLabelInputFieldHorizontalPadding -
                      kFieldExtraViewHorizontalPadding - short_extra_view_width;
  columns_short->AddPaddingColumn(0, short_padding);

  // Column set for long fields.
  views::ColumnSet* columns_long = editor_layout->AddColumnSet(1);
  columns_long->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                          0, views::GridLayout::FIXED, kLabelWidth, 0);
  columns_long->AddPaddingColumn(0, kLabelInputFieldHorizontalPadding);
  // The field view column stretches.
  columns_long->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                          1, views::GridLayout::USE_PREF, 0, 0);
  columns_long->AddPaddingColumn(0, kFieldExtraViewHorizontalPadding);
  // The extra field view column is fixed size, computed from the largest
  // extra view.
  int long_extra_view_width =
      ComputeWidestExtraViewWidth(EditorField::LengthHint::HINT_LONG);
  columns_long->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                          0, views::GridLayout::FIXED, long_extra_view_width,
                          0);
  // The padding at the end is fixed, computed to make sure the long field
  // maintains its minimum width.
  int long_padding = kDialogMinWidth - kLongFieldMinimumWidth - kLabelWidth -
                     (2 * kPaymentRequestRowHorizontalInsets) -
                     kLabelInputFieldHorizontalPadding -
                     kFieldExtraViewHorizontalPadding - long_extra_view_width;
  columns_long->AddPaddingColumn(0, long_padding);

  for (const auto& field : GetFieldDefinitions())
    CreateInputField(editor_layout.get(), field);

  // Adds the "* indicates a required field" label in "disabled" grey text.
  std::unique_ptr<views::Label> required_field = base::MakeUnique<views::Label>(
      l10n_util::GetStringUTF16(IDS_PAYMENTS_REQUIRED_FIELD_MESSAGE));
  required_field->SetDisabledColor(
      required_field->GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_LabelDisabledColor));
  required_field->SetEnabled(false);

  views::ColumnSet* required_field_columns = editor_layout->AddColumnSet(2);
  required_field_columns->AddColumn(views::GridLayout::LEADING,
                                    views::GridLayout::CENTER, 1,
                                    views::GridLayout::USE_PREF, 0, 0);
  editor_layout->StartRow(0, 2);
  editor_layout->AddView(required_field.release());

  editor_view->SetLayoutManager(editor_layout.release());

  return editor_view;
}

// Each input field is a 4-quadrant grid.
// +----------------------------------------------------------+
// | Field Label           | Input field (textfield/combobox) |
// |_______________________|__________________________________|
// |   (empty)             | Error label                      |
// +----------------------------------------------------------+
void EditorViewController::CreateInputField(views::GridLayout* layout,
                                            const EditorField& field) {
  int column_set =
      field.length_hint == EditorField::LengthHint::HINT_SHORT ? 0 : 1;

  // This is the top padding for every row.
  constexpr int kInputRowSpacing = 6;
  layout->StartRowWithPadding(0, column_set, 0, kInputRowSpacing);

  std::unique_ptr<views::Label> label = base::MakeUnique<views::Label>(
      field.required ? field.label + base::ASCIIToUTF16("*") : field.label);

  label->SetMultiLine(true);
  layout->AddView(label.release());

  constexpr int kInputFieldHeight = 28;
  if (field.control_type == EditorField::ControlType::TEXTFIELD) {
    ValidatingTextfield* text_field =
        new ValidatingTextfield(CreateValidationDelegate(field));
    text_field->SetText(GetInitialValueForType(field.type));
    text_field->set_controller(this);
    // Using autofill field type as a view ID (for testing).
    text_field->set_id(static_cast<int>(field.type));
    text_fields_.insert(std::make_pair(text_field, field));

    // TODO(crbug.com/718582): Make the initial focus the first incomplete/empty
    // field.
    if (!first_field_view_)
      first_field_view_ = text_field;

    // |text_field| will now be owned by |row|.
    layout->AddView(text_field, 1, 1, views::GridLayout::FILL,
                    views::GridLayout::FILL, 0, kInputFieldHeight);
  } else if (field.control_type == EditorField::ControlType::COMBOBOX) {
    ValidatingCombobox* combobox = new ValidatingCombobox(
        GetComboboxModelForType(field.type), CreateValidationDelegate(field));
    base::string16 initial_value = GetInitialValueForType(field.type);
    if (!initial_value.empty())
      combobox->SelectValue(initial_value);
    // Using autofill field type as a view ID.
    combobox->set_id(static_cast<int>(field.type));
    combobox->set_listener(this);
    comboboxes_.insert(std::make_pair(combobox, field));

    if (!first_field_view_)
      first_field_view_ = combobox;

    // |combobox| will now be owned by |row|.
    layout->AddView(combobox, 1, 1, views::GridLayout::FILL,
                    views::GridLayout::FILL, 0, kInputFieldHeight);
  } else {
    // Custom field view will now be owned by |row|. And it must be valid since
    // the derived class specified a custom view for this field.
    std::unique_ptr<views::View> field_view = CreateCustomFieldView(field.type);
    DCHECK(field_view);
    layout->AddView(field_view.release());
  }

  // If an extra view needs to go alongside the input field view, add it to the
  // last column.
  std::unique_ptr<views::View> extra_view = CreateExtraViewForField(field.type);
  if (extra_view)
    layout->AddView(extra_view.release());

  layout->StartRow(0, column_set);
  layout->SkipColumns(1);
  std::unique_ptr<views::View> error_label_view =
      base::MakeUnique<views::View>();
  error_label_view->SetLayoutManager(new views::FillLayout);
  error_labels_[field] = error_label_view.get();
  layout->AddView(error_label_view.release());

  // Bottom padding for the row.
  layout->AddPaddingRow(0, kInputRowSpacing);
}

int EditorViewController::ComputeWidestExtraViewWidth(
    EditorField::LengthHint size) {
  int widest_column_width = 0;

  for (const auto& field : GetFieldDefinitions()) {
    if (field.length_hint != size)
      continue;

    std::unique_ptr<views::View> extra_view =
        CreateExtraViewForField(field.type);
    if (!extra_view)
      continue;
    widest_column_width =
        std::max(extra_view->GetPreferredSize().width(), widest_column_width);
  }
  return widest_column_width;
}

}  // namespace payments
