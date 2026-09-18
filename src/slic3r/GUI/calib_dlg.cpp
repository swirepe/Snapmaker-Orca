#include "calib_dlg.hpp"
#include "GUI_App.hpp"
#include "MsgDialog.hpp"
#include "I18N.hpp"
#include <wx/dcgraph.h>
#include "MainFrame.hpp"
#include "Tab.hpp"
#include "Widgets/DialogButtons.hpp"
#include <string>

namespace Slic3r { namespace GUI {

namespace {

void ParseStringValues(std::string str, std::vector<double> &vec)
{
    vec.clear();
    std::replace(str.begin(), str.end(), ',', ' ');
    std::istringstream inss(str);
    std::copy_if(std::istream_iterator<int>(inss), std::istream_iterator<int>(), std::back_inserter(vec),
                 [](int x){ return x > 0; });
}

int GetTextMax(wxWindow* parent, const std::vector<wxString>& labels)
{
    wxSize text_size;
    for (wxString label : labels)
        text_size.IncTo(parent->GetTextExtent(label));
    return text_size.x + parent->FromDIP(10);
}

}

PA_Calibration_Dlg::PA_Calibration_Dlg(wxWindow* parent, wxWindowID id, Plater* plater)
    : DPIDialog(parent, id, _L("PA Calibration"), wxDefaultPosition, parent->FromDIP(wxSize(-1, 280)), wxDEFAULT_DIALOG_STYLE), m_plater(plater)
{
    SetBackgroundColour(*wxWHITE); // make sure background color set for dialog
    SetForegroundColour(wxColour("#363636"));
    SetFont(Label::Body_14);

    wxBoxSizer* v_sizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(v_sizer);

    // Extruder type Radio Group
    auto labeled_box_type = new LabeledStaticBox(this, _L("Extruder type"));
    auto type_box = new wxStaticBoxSizer(labeled_box_type, wxHORIZONTAL);

    m_rbExtruderType = new RadioGroup(this, {_L("DDE"), _L("Bowden")}, wxHORIZONTAL);
    type_box->Add(m_rbExtruderType, 0, wxALL | wxEXPAND, FromDIP(4));
    v_sizer->Add(type_box, 0, wxTOP | wxRIGHT | wxLEFT | wxEXPAND, FromDIP(10));

    // Method Radio Group
    auto labeled_box_method = new LabeledStaticBox(this, _L("Method"));
    auto method_box = new wxStaticBoxSizer(labeled_box_method, wxHORIZONTAL);

	m_rbMethod = new RadioGroup(this, { _L("PA Tower"), _L("PA Line"), _L("PA Pattern") }, wxHORIZONTAL);
    method_box->Add(m_rbMethod, 0, wxALL | wxEXPAND, FromDIP(4));
    v_sizer->Add(method_box, 0, wxTOP | wxRIGHT | wxLEFT | wxEXPAND, FromDIP(10));

    // Settings
    wxString start_pa_str    = _L("Start PA: ");
    wxString end_pa_str      = _L("End PA: ");
    wxString PA_step_str     = _L("PA step: ");
    wxString sp_accel_str    = _L("Accelerations: ");
    wxString sp_speed_str    = _L("Speeds: ");
    wxString cb_print_no_str = _L("Print numbers");

    int text_max = GetTextMax(this, std::vector<wxString>{start_pa_str, end_pa_str, PA_step_str, sp_accel_str, sp_speed_str, cb_print_no_str});

    auto st_size = FromDIP(wxSize(text_max, -1));
    auto ti_size = FromDIP(wxSize(120, -1));

    LabeledStaticBox* stb = new LabeledStaticBox(this, _L("Settings"));
    wxStaticBoxSizer* settings_sizer = new wxStaticBoxSizer(stb, wxVERTICAL);

    settings_sizer->AddSpacer(FromDIP(5));

    // start PA
    auto start_PA_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto start_pa_text = new wxStaticText(this, wxID_ANY, start_pa_str, wxDefaultPosition, st_size, wxALIGN_LEFT);
    m_tiStartPA = new TextInput(this, "", "", "", wxDefaultPosition, ti_size, wxTE_PROCESS_ENTER);
    m_tiStartPA->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
	start_PA_sizer->Add(start_pa_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    start_PA_sizer->Add(m_tiStartPA  , 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    settings_sizer->Add(start_PA_sizer, 0, wxLEFT, FromDIP(3));

    // end PA
    auto end_PA_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto end_pa_text = new wxStaticText(this, wxID_ANY, end_pa_str, wxDefaultPosition, st_size, wxALIGN_LEFT);
    m_tiEndPA = new TextInput(this, "", "", "", wxDefaultPosition, ti_size, wxTE_PROCESS_ENTER);
    m_tiStartPA->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
    end_PA_sizer->Add(end_pa_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    end_PA_sizer->Add(m_tiEndPA  , 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    settings_sizer->Add(end_PA_sizer, 0, wxLEFT, FromDIP(3));

    // PA step
    auto PA_step_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto PA_step_text = new wxStaticText(this, wxID_ANY, PA_step_str, wxDefaultPosition, st_size, wxALIGN_LEFT);
    m_tiPAStep = new TextInput(this, "", "", "", wxDefaultPosition, ti_size, wxTE_PROCESS_ENTER);
    m_tiStartPA->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
    PA_step_sizer->Add(PA_step_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    PA_step_sizer->Add(m_tiPAStep  , 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    settings_sizer->Add(PA_step_sizer, 0, wxLEFT, FromDIP(3));

    // Print Numbers
    wxBoxSizer* cb_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto cb_title = new wxStaticText(this, wxID_ANY, cb_print_no_str, wxDefaultPosition, st_size, 0);
    m_cbPrintNum = new ::CheckBox(this);
    m_cbPrintNum->SetValue(false);
    m_cbPrintNum->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent& e) {
        (m_params.print_numbers) = (m_params.print_numbers) ? false : true;
        e.Skip();
    });
    cb_sizer->Add(cb_title      , 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    cb_sizer->Add(m_cbPrintNum  , 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    settings_sizer->Add(cb_sizer, 0, wxLEFT | wxTOP | wxBOTTOM, FromDIP(3));

    wxTextValidator val_list_validator(wxFILTER_INCLUDE_CHAR_LIST);
    val_list_validator.SetCharIncludes(wxString("0123456789,"));

    auto sp_accel_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto sp_accel_text = new wxStaticText(this, wxID_ANY, sp_accel_str, wxDefaultPosition, st_size, wxALIGN_LEFT);
    m_tiBMAccels = new TextInput(this, "", "", "", wxDefaultPosition, ti_size, wxTE_PROCESS_ENTER);
    m_tiBMAccels->SetToolTip(_L("Comma-separated list of printing accelerations"));
    m_tiBMAccels->GetTextCtrl()->SetValidator(val_list_validator);
    sp_accel_sizer->Add(sp_accel_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    sp_accel_sizer->Add(m_tiBMAccels , 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    settings_sizer->Add(sp_accel_sizer, 0, wxLEFT, FromDIP(3));

    auto sp_speed_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto sp_speed_text = new wxStaticText(this, wxID_ANY, sp_speed_str, wxDefaultPosition, st_size, wxALIGN_LEFT);
    m_tiBMSpeeds = new TextInput(this, "", "", "", wxDefaultPosition, ti_size, wxTE_PROCESS_ENTER);
    m_tiBMSpeeds->SetToolTip(_L("Comma-separated list of printing speeds"));
    m_tiBMSpeeds->GetTextCtrl()->SetValidator(val_list_validator);
    sp_speed_sizer->Add(sp_speed_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    sp_speed_sizer->Add(m_tiBMSpeeds , 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    settings_sizer->Add(sp_speed_sizer, 0, wxLEFT, FromDIP(3));

    v_sizer->Add(settings_sizer, 0, wxTOP | wxRIGHT | wxLEFT | wxEXPAND, FromDIP(10));
    v_sizer->AddSpacer(FromDIP(5));

    auto dlg_btns = new DialogButtons(this, {"OK"});
    v_sizer->Add(dlg_btns , 0, wxEXPAND);

    dlg_btns->GetOK()->Bind(wxEVT_BUTTON, &PA_Calibration_Dlg::on_start, this);

    PA_Calibration_Dlg::reset_params();

    // Connect Events
    m_rbExtruderType->Connect(wxEVT_COMMAND_RADIOBOX_SELECTED, wxCommandEventHandler(PA_Calibration_Dlg::on_extruder_type_changed), NULL, this);
    m_rbMethod->Connect(wxEVT_COMMAND_RADIOBOX_SELECTED, wxCommandEventHandler(PA_Calibration_Dlg::on_method_changed), NULL, this);
    this->Connect(wxEVT_SHOW, wxShowEventHandler(PA_Calibration_Dlg::on_show));
    
    wxGetApp().UpdateDlgDarkUI(this);

    Layout();
    Fit();
}

PA_Calibration_Dlg::~PA_Calibration_Dlg() {
    // Disconnect Events
    m_rbExtruderType->Disconnect(wxEVT_COMMAND_RADIOBOX_SELECTED, wxCommandEventHandler(PA_Calibration_Dlg::on_extruder_type_changed), NULL, this);
    m_rbMethod->Disconnect(wxEVT_COMMAND_RADIOBOX_SELECTED, wxCommandEventHandler(PA_Calibration_Dlg::on_method_changed), NULL, this);
}

void PA_Calibration_Dlg::reset_params() {
    bool isDDE = m_rbExtruderType->GetSelection() == 0 ? true : false;
    int method = m_rbMethod->GetSelection();

    m_tiStartPA->GetTextCtrl()->SetValue(wxString::FromDouble(0.0));

    switch (method) {
        case 1:
            m_params.mode = CalibMode::Calib_PA_Line;
            m_tiEndPA->GetTextCtrl()->SetValue(wxString::FromDouble(0.1));
            m_tiPAStep->GetTextCtrl()->SetValue(wxString::FromDouble(0.002));
            m_cbPrintNum->SetValue(true);
            m_cbPrintNum->Enable(true);
            m_tiBMAccels->Enable(false);
            m_tiBMSpeeds->Enable(false);
            break;
        case 2:
            m_params.mode = CalibMode::Calib_PA_Pattern;
            m_tiEndPA->GetTextCtrl()->SetValue(wxString::FromDouble(0.08));
            m_tiPAStep->GetTextCtrl()->SetValue(wxString::FromDouble(0.005));
            m_cbPrintNum->SetValue(true);
            m_cbPrintNum->Enable(false);
            m_tiBMAccels->Enable(true);
            m_tiBMSpeeds->Enable(true);
            break;
        default:
            m_params.mode = CalibMode::Calib_PA_Tower;
            m_tiEndPA->GetTextCtrl()->SetValue(wxString::FromDouble(0.1));
            m_tiPAStep->GetTextCtrl()->SetValue(wxString::FromDouble(0.002));
            m_cbPrintNum->SetValue(false);
            m_cbPrintNum->Enable(false);
            m_tiBMAccels->Enable(false);
            m_tiBMSpeeds->Enable(false);
            break;
    }

    if (!isDDE) {
        m_tiEndPA->GetTextCtrl()->SetValue(wxString::FromDouble(1.0));
        
        if (m_params.mode == CalibMode::Calib_PA_Pattern) {
            m_tiPAStep->GetTextCtrl()->SetValue(wxString::FromDouble(0.05));
        } else {
            m_tiPAStep->GetTextCtrl()->SetValue(wxString::FromDouble(0.02));
        }
    }
}

void PA_Calibration_Dlg::on_start(wxCommandEvent& event) { 
    bool read_double = false;
    read_double = m_tiStartPA->GetTextCtrl()->GetValue().ToDouble(&m_params.start);
    read_double = read_double && m_tiEndPA->GetTextCtrl()->GetValue().ToDouble(&m_params.end);
    read_double = read_double && m_tiPAStep->GetTextCtrl()->GetValue().ToDouble(&m_params.step);
    if (!read_double || m_params.start < 0 || m_params.step < EPSILON || m_params.end < m_params.start + m_params.step) {
        MessageDialog msg_dlg(nullptr, _L("Please input valid values:\nStart PA: >= 0.0\nEnd PA: > Start PA\nPA step: >= 0.001"), wxEmptyString, wxICON_WARNING | wxOK);
        msg_dlg.ShowModal();
        return;
    }

    switch (m_rbMethod->GetSelection()) {
        case 1:
            m_params.mode = CalibMode::Calib_PA_Line;
            break;
        case 2:
            m_params.mode = CalibMode::Calib_PA_Pattern;
            break;
        default:
            m_params.mode = CalibMode::Calib_PA_Tower;
    }

    m_params.print_numbers = m_cbPrintNum->GetValue();
    ParseStringValues(m_tiBMAccels->GetTextCtrl()->GetValue().ToStdString(), m_params.accelerations);
    ParseStringValues(m_tiBMSpeeds->GetTextCtrl()->GetValue().ToStdString(), m_params.speeds);

    m_plater->calib_pa(m_params);
    EndModal(wxID_OK);

}
void PA_Calibration_Dlg::on_extruder_type_changed(wxCommandEvent& event) { 
    PA_Calibration_Dlg::reset_params();
    event.Skip(); 
}
void PA_Calibration_Dlg::on_method_changed(wxCommandEvent& event) { 
    PA_Calibration_Dlg::reset_params();
    event.Skip(); 
}

void PA_Calibration_Dlg::on_dpi_changed(const wxRect& suggested_rect) {
    this->Refresh(); 
    Fit();
}

void PA_Calibration_Dlg::on_show(wxShowEvent& event) {
    PA_Calibration_Dlg::reset_params();
}

// Temp calib dlg
//
enum FILAMENT_TYPE : int
{
    tPLA = 0,
    tABS_ASA,
    tPETG,
    tPCTG,
    tTPU,
    tPA_CF,
    tPET_CF,
    tCustom
};

Temp_Calibration_Dlg::Temp_Calibration_Dlg(wxWindow* parent, wxWindowID id, Plater* plater)
    : DPIDialog(parent, id, _L("Temperature calibration"), wxDefaultPosition, parent->FromDIP(wxSize(-1, 280)), wxDEFAULT_DIALOG_STYLE), m_plater(plater)
{
    SetBackgroundColour(*wxWHITE); // make sure background color set for dialog
    SetForegroundColour(wxColour("#363636"));
    SetFont(Label::Body_14);

    wxBoxSizer* v_sizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(v_sizer);

    // Method Radio Group
    auto labeled_box_method = new LabeledStaticBox(this, _L("Filament type"));
    auto method_box = new wxStaticBoxSizer(labeled_box_method, wxHORIZONTAL);

	m_rbFilamentType = new RadioGroup(this, { _L("PLA"), _L("ABS/ASA"), _L("PETG"), _L("PCTG"), _L("TPU"), _L("PA-CF"), _L("PET-CF"), _L("Custom") }, wxVERTICAL, 2);
    method_box->Add(m_rbFilamentType, 0, wxALL | wxEXPAND, FromDIP(4));
    v_sizer->Add(method_box, 0, wxTOP | wxRIGHT | wxLEFT | wxEXPAND, FromDIP(10));

    // Settings
    wxString start_temp_str = _L("Start temp: ");
    wxString end_temp_str   = _L("End temp: ");
    wxString temp_step_str  = _L("Temp step: ");
    int text_max = GetTextMax(this, std::vector<wxString>{start_temp_str, end_temp_str, temp_step_str});

    auto st_size = FromDIP(wxSize(text_max, -1));
    auto ti_size = FromDIP(wxSize(120, -1));

    LabeledStaticBox* stb = new LabeledStaticBox(this, _L("Settings"));
    wxStaticBoxSizer* settings_sizer = new wxStaticBoxSizer(stb, wxVERTICAL);

    settings_sizer->AddSpacer(FromDIP(5));

    // start temp
    auto start_temp_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto start_temp_text = new wxStaticText(this, wxID_ANY, start_temp_str, wxDefaultPosition, st_size, wxALIGN_LEFT);
    m_tiStart = new TextInput(this, std::to_string(230), wxString::FromUTF8("\u2103") /* °C */, "", wxDefaultPosition, ti_size);
    m_tiStart->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
    start_temp_sizer->Add(start_temp_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    start_temp_sizer->Add(m_tiStart      , 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    settings_sizer->Add(start_temp_sizer, 0, wxLEFT, FromDIP(3));

    // end temp
    auto end_temp_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto end_temp_text = new wxStaticText(this, wxID_ANY, end_temp_str, wxDefaultPosition, st_size, wxALIGN_LEFT);
    m_tiEnd = new TextInput(this, std::to_string(190), wxString::FromUTF8("\u2103") /* °C */, "", wxDefaultPosition, ti_size);
    m_tiStart->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
    end_temp_sizer->Add(end_temp_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    end_temp_sizer->Add(m_tiEnd      , 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    settings_sizer->Add(end_temp_sizer, 0, wxLEFT, FromDIP(3));

    // temp step
    auto temp_step_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto temp_step_text = new wxStaticText(this, wxID_ANY, temp_step_str, wxDefaultPosition, st_size, wxALIGN_LEFT);
    m_tiStep = new TextInput(this, wxString::FromDouble(5), wxString::FromUTF8("\u2103") /* °C */, "", wxDefaultPosition, ti_size);
    m_tiStart->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
    m_tiStep->Enable(false);
    temp_step_sizer->Add(temp_step_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    temp_step_sizer->Add(m_tiStep      , 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    settings_sizer->Add(temp_step_sizer, 0, wxLEFT, FromDIP(3));

    settings_sizer->AddSpacer(FromDIP(5));

    v_sizer->Add(settings_sizer, 0, wxTOP | wxRIGHT | wxLEFT | wxEXPAND, FromDIP(10));
    v_sizer->AddSpacer(FromDIP(5));

    auto dlg_btns = new DialogButtons(this, {"OK"});
    v_sizer->Add(dlg_btns , 0, wxEXPAND);

    dlg_btns->GetOK()->Bind(wxEVT_BUTTON, &Temp_Calibration_Dlg::on_start, this);

    m_rbFilamentType->Connect(wxEVT_COMMAND_RADIOBOX_SELECTED, wxCommandEventHandler(Temp_Calibration_Dlg::on_filament_type_changed), NULL, this);

    wxGetApp().UpdateDlgDarkUI(this);

    Layout();
    Fit();

    auto validate_text = [](TextInput* ti){
        unsigned long t = 0;
        if(!ti->GetTextCtrl()->GetValue().ToULong(&t))
            return;
        if(t> 350 || t < 170){
            MessageDialog msg_dlg(nullptr, wxString::Format(L"Supported range: 170%s - 350%s",
                wxString::FromUTF8("\u2103") /* °C */, wxString::FromUTF8("\u2103") /* °C */),
                wxEmptyString, wxICON_WARNING | wxOK);
            msg_dlg.ShowModal();
            if(t > 350)
                t = 350;
            else
                t = 170;
        }
        t = (t / 5) * 5;
        ti->GetTextCtrl()->SetValue(std::to_string(t));
    };

    m_tiStart->GetTextCtrl()->Bind(wxEVT_KILL_FOCUS, [&](wxFocusEvent &e) {
        validate_text(this->m_tiStart);
        e.Skip();
        });

    m_tiEnd->GetTextCtrl()->Bind(wxEVT_KILL_FOCUS, [&](wxFocusEvent &e) {
        validate_text(this->m_tiEnd);
        e.Skip();
        });

    
}

Temp_Calibration_Dlg::~Temp_Calibration_Dlg() {
    // Disconnect Events
    m_rbFilamentType->Disconnect(wxEVT_COMMAND_RADIOBOX_SELECTED, wxCommandEventHandler(Temp_Calibration_Dlg::on_filament_type_changed), NULL, this);
}

void Temp_Calibration_Dlg::on_start(wxCommandEvent& event) {
    bool read_long = false;
    unsigned long start=0,end=0;
    read_long = m_tiStart->GetTextCtrl()->GetValue().ToULong(&start);
    read_long = read_long && m_tiEnd->GetTextCtrl()->GetValue().ToULong(&end);

    if (!read_long || start > 350 || end < 170  || end > (start - 5)) {
        MessageDialog msg_dlg(nullptr, _L("Please input valid values:\nStart temp: <= 350\nEnd temp: >= 170\nStart temp > End temp + 5"), wxEmptyString, wxICON_WARNING | wxOK);
        msg_dlg.ShowModal();
        return;
    }
    m_params.start = start;
    m_params.end = end;
    m_params.mode = CalibMode::Calib_Temp_Tower;
    m_plater->calib_temp(m_params);
    EndModal(wxID_OK);

}

void Temp_Calibration_Dlg::on_filament_type_changed(wxCommandEvent& event) {
    int selection = event.GetSelection();
    unsigned long start = 0, end = 0;
    switch(selection)
    {
        case tABS_ASA:
            start = 270;
            end = 230;
            break;
        case tPETG:
            start = 250;
            end = 230;
            break;
	case tPCTG:
            start = 280;
            end = 240;
            break;
        case tTPU:
            start = 240;
            end = 210;
            break;
        case tPA_CF:
            start = 320;
            end = 280;
            break;
        case tPET_CF:
            start = 320;
            end = 280;
            break;
        case tPLA:
        case tCustom:
            start = 230;
            end = 190;
            break;
    }
    
    m_tiEnd->GetTextCtrl()->SetValue(std::to_string(end));
    m_tiStart->GetTextCtrl()->SetValue(std::to_string(start));
    event.Skip();
}

void Temp_Calibration_Dlg::on_dpi_changed(const wxRect& suggested_rect) {
    this->Refresh();
    Fit();

}

Thermal_Pattern_Calibration_Dlg::Thermal_Pattern_Calibration_Dlg(wxWindow *parent, wxWindowID id, Plater *plater)
    : DPIDialog(parent, id, _L("Thermal surface patterning calibration"), wxDefaultPosition,
                parent->FromDIP(wxSize(-1, 340)), wxDEFAULT_DIALOG_STYLE),
      m_plater(plater)
{
    SetBackgroundColour(*wxWHITE);
    SetForegroundColour(wxColour("#363636"));
    SetFont(Label::Body_14);

    auto *outer = new wxBoxSizer(wxVERTICAL);
    SetSizer(outer);
    auto *box = new LabeledStaticBox(this, _L("Calibration range"));
    auto *settings = new wxStaticBoxSizer(box, wxVERTICAL);
    const wxSize input_size = FromDIP(wxSize(120, -1));

    auto add_input = [this, settings, input_size](const wxString &label, const wxString &value, const wxString &unit,
                                                  TextInput *&input) {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        auto *text = new wxStaticText(this, wxID_ANY, label, wxDefaultPosition, FromDIP(wxSize(190, -1)), wxALIGN_LEFT);
        input = new TextInput(this, value, unit, "", wxDefaultPosition, input_size);
        input->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
        row->Add(text, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
        row->Add(input, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
        settings->Add(row, 0, wxLEFT, FromDIP(3));
    };

    const DynamicPrintConfig full_config = wxGetApp().preset_bundle->full_config();
    const auto *base_temperatures = full_config.option<ConfigOptionInts>("nozzle_temperature");
    const auto *pattern_steps = full_config.option<ConfigOptionFloats>("thermal_pattern_temperature_step");
    const int base_temperature = base_temperatures == nullptr || base_temperatures->values.empty() ? 210 : base_temperatures->values.front();
    const double pattern_step = pattern_steps == nullptr || pattern_steps->values.empty() ? 10.0 : pattern_steps->values.front();

    settings->AddSpacer(FromDIP(5));
    add_input(_L("Base temperature:"), wxString::Format("%d", base_temperature), wxString::FromUTF8("℃"), m_ti_base);
    add_input(_L("Temperature step:"), wxString::Format("%.1f", pattern_step), wxString::FromUTF8("℃"), m_ti_step);
    add_input(_L("Maximum level:"), "6", "", m_ti_levels);
    add_input(_L("Band height:"), "5", "mm", m_ti_band_height);
    settings->AddSpacer(FromDIP(5));
    outer->Add(settings, 0, wxTOP | wxRIGHT | wxLEFT | wxEXPAND, FromDIP(10));

    auto *description = new wxStaticText(
        this, wxID_ANY,
        _L("Generate a wall tower and stepped top-surface swatches. After printing, enter the selected range and apply it to a new filament preset."));
    description->Wrap(FromDIP(420));
    outer->Add(description, 0, wxALL | wxEXPAND, FromDIP(12));

    auto *buttons = new DialogButtons(this, {"Apply", "OK", "Cancel"});
    buttons->GetOK()->SetLabel(_L("Generate test"));
    buttons->GetAPPLY()->SetLabel(_L("Apply and save filament"));
    buttons->GetOK()->Bind(wxEVT_BUTTON, &Thermal_Pattern_Calibration_Dlg::on_generate, this);
    buttons->GetAPPLY()->Bind(wxEVT_BUTTON, &Thermal_Pattern_Calibration_Dlg::on_apply, this);
    buttons->GetCANCEL()->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
    outer->Add(buttons, 0, wxEXPAND);

    wxGetApp().UpdateDlgDarkUI(this);
    Layout();
    Fit();
}

bool Thermal_Pattern_Calibration_Dlg::read_params(Calib_Params &params, bool warn_about_filament_limit)
{
    long levels = 0;
    bool valid = m_ti_base->GetTextCtrl()->GetValue().ToDouble(&params.start) &&
                 m_ti_step->GetTextCtrl()->GetValue().ToDouble(&params.step) &&
                 m_ti_band_height->GetTextCtrl()->GetValue().ToDouble(&params.thermal_band_height) &&
                 m_ti_levels->GetTextCtrl()->GetValue().ToLong(&levels);
    if (!valid || params.start < 0. || params.step <= 0. || params.thermal_band_height < 0.4 || levels < 1 || levels > 20) {
        MessageDialog(this, _L("Enter a positive temperature step, 1-20 levels, and a band height of at least 0.4 mm."),
                      wxEmptyString, wxICON_WARNING | wxOK).ShowModal();
        return false;
    }

    params.thermal_max_level = static_cast<int>(levels);
    params.end = params.start + params.step * levels;
    params.mode = CalibMode::Calib_Thermal_Pattern;

    const DynamicPrintConfig full_config = wxGetApp().preset_bundle->full_config();
    const auto *machine_limit = full_config.option<ConfigOptionInts>("machine_max_nozzle_temperature");
    const int hardware_max = machine_limit == nullptr || machine_limit->values.empty() ? 300 : machine_limit->values.front();
    if (params.end > hardware_max) {
        MessageDialog(this, wxString::Format(_L("The requested maximum temperature %.0f°C exceeds the machine limit of %d°C."),
                                             params.end, hardware_max),
                      wxEmptyString, wxICON_ERROR | wxOK).ShowModal();
        return false;
    }

    if (warn_about_filament_limit) {
        const auto *filament_limit = full_config.option<ConfigOptionInts>("nozzle_temperature_range_high");
        const int recommended_max = filament_limit == nullptr || filament_limit->values.empty() ? hardware_max : filament_limit->values.front();
        if (params.end > recommended_max) {
            MessageDialog warning(
                this,
                wxString::Format(_L("The requested maximum temperature %.0f°C exceeds this filament preset's recommended maximum of %d°C. Continue?"),
                                 params.end, recommended_max),
                _L("Thermal pattern temperature warning"), wxICON_WARNING | wxYES_NO | wxNO_DEFAULT);
            if (warning.ShowModal() != wxID_YES)
                return false;
        }
    }
    return true;
}

void Thermal_Pattern_Calibration_Dlg::on_generate(wxCommandEvent &)
{
    Calib_Params params;
    if (!read_params(params, true))
        return;
    m_plater->calib_thermal_pattern(params);
    EndModal(wxID_OK);
}

void Thermal_Pattern_Calibration_Dlg::on_apply(wxCommandEvent &)
{
    Calib_Params params;
    if (!read_params(params, true))
        return;

    DynamicPrintConfig &config = wxGetApp().preset_bundle->filaments.get_edited_preset().config;
    config.set_key_value("thermal_pattern_enabled", new ConfigOptionBools(1, true));
    config.set_key_value("thermal_pattern_temperature_step", new ConfigOptionFloats(1, params.step));
    config.set_key_value("thermal_pattern_max_temperature", new ConfigOptionInts(1, static_cast<int>(std::lround(params.end))));
    Tab *filament_tab = wxGetApp().get_tab(Preset::TYPE_FILAMENT);
    filament_tab->update_dirty();
    filament_tab->reload_config();
    EndModal(wxID_OK);
    wxGetApp().CallAfter([filament_tab]() { filament_tab->save_preset(); });
}

void Thermal_Pattern_Calibration_Dlg::on_dpi_changed(const wxRect &)
{
    Refresh();
    Fit();
}


// MaxVolumetricSpeed_Test_Dlg
//

MaxVolumetricSpeed_Test_Dlg::MaxVolumetricSpeed_Test_Dlg(wxWindow* parent, wxWindowID id, Plater* plater)
    : DPIDialog(parent, id, _L("Max volumetric speed test"), wxDefaultPosition, parent->FromDIP(wxSize(-1, 280)), wxDEFAULT_DIALOG_STYLE), m_plater(plater)
{
    SetBackgroundColour(*wxWHITE); // make sure background color set for dialog
    SetForegroundColour(wxColour("#363636"));
    SetFont(Label::Body_14);

    wxBoxSizer* v_sizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(v_sizer);

    // Settings
    wxString start_vol_str = _L("Start volumetric speed: ");
    wxString end_vol_str   = _L("End volumetric speed: ");
    wxString vol_step_str  = _L("Step") + ": ";
    int text_max = GetTextMax(this, std::vector<wxString>{start_vol_str, end_vol_str, vol_step_str});

    auto st_size = FromDIP(wxSize(text_max, -1));
    auto ti_size = FromDIP(wxSize(120, -1));

    LabeledStaticBox* stb = new LabeledStaticBox(this, _L("Settings"));
    wxStaticBoxSizer* settings_sizer = new wxStaticBoxSizer(stb, wxVERTICAL);

    settings_sizer->AddSpacer(FromDIP(5));

    // start vol
    auto start_vol_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto start_vol_text = new wxStaticText(this, wxID_ANY, start_vol_str, wxDefaultPosition, st_size, wxALIGN_LEFT);
    m_tiStart = new TextInput(this, std::to_string(5), wxString::FromUTF8("mm³/s"), "", wxDefaultPosition, ti_size);
    m_tiStart->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));

    start_vol_sizer->Add(start_vol_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    start_vol_sizer->Add(m_tiStart     , 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    settings_sizer->Add(start_vol_sizer, 0, wxLEFT, FromDIP(3));

    // end vol
    auto end_vol_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto end_vol_text = new wxStaticText(this, wxID_ANY, end_vol_str, wxDefaultPosition, st_size, wxALIGN_LEFT);
    m_tiEnd = new TextInput(this, std::to_string(20), wxString::FromUTF8("mm³/s"), "", wxDefaultPosition, ti_size);
    m_tiStart->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
    end_vol_sizer->Add(end_vol_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    end_vol_sizer->Add(m_tiEnd     , 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    settings_sizer->Add(end_vol_sizer, 0, wxLEFT, FromDIP(3));

    // vol step
    auto vol_step_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto vol_step_text = new wxStaticText(this, wxID_ANY, vol_step_str, wxDefaultPosition, st_size, wxALIGN_LEFT);
    m_tiStep = new TextInput(this, wxString::FromDouble(0.5), wxString::FromUTF8("mm³/s"), "", wxDefaultPosition, ti_size);
    m_tiStart->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
    vol_step_sizer->Add(vol_step_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    vol_step_sizer->Add(m_tiStep     , 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    settings_sizer->Add(vol_step_sizer, 0, wxLEFT, FromDIP(3));

    v_sizer->Add(settings_sizer, 0, wxTOP | wxRIGHT | wxLEFT | wxEXPAND, FromDIP(10));
    v_sizer->AddSpacer(FromDIP(5));

    auto dlg_btns = new DialogButtons(this, {"OK"});
    v_sizer->Add(dlg_btns , 0, wxEXPAND);

    dlg_btns->GetOK()->Bind(wxEVT_BUTTON, &MaxVolumetricSpeed_Test_Dlg::on_start, this);

    wxGetApp().UpdateDlgDarkUI(this);

    Layout();
    Fit();
}

MaxVolumetricSpeed_Test_Dlg::~MaxVolumetricSpeed_Test_Dlg() {
    // Disconnect Events
}

void MaxVolumetricSpeed_Test_Dlg::on_start(wxCommandEvent& event) {
    bool read_double = false;
    read_double = m_tiStart->GetTextCtrl()->GetValue().ToDouble(&m_params.start);
    read_double = read_double && m_tiEnd->GetTextCtrl()->GetValue().ToDouble(&m_params.end);
    read_double = read_double && m_tiStep->GetTextCtrl()->GetValue().ToDouble(&m_params.step);

    if (!read_double || m_params.start <= 0 || m_params.step <= 0 || m_params.end < (m_params.start + m_params.step)) {
        MessageDialog msg_dlg(nullptr, _L("Please input valid values:\nstart > 0\nstep >= 0\nend > start + step"), wxEmptyString, wxICON_WARNING | wxOK);
        msg_dlg.ShowModal();
        return;
    }

    m_params.mode = CalibMode::Calib_Vol_speed_Tower;
    m_plater->calib_max_vol_speed(m_params);
    EndModal(wxID_OK);

}

void MaxVolumetricSpeed_Test_Dlg::on_dpi_changed(const wxRect& suggested_rect) {
    this->Refresh();
    Fit();

}


// VFA_Test_Dlg
//

VFA_Test_Dlg::VFA_Test_Dlg(wxWindow* parent, wxWindowID id, Plater* plater)
    : DPIDialog(parent, id, _L("VFA test"), wxDefaultPosition, parent->FromDIP(wxSize(-1, 280)), wxDEFAULT_DIALOG_STYLE)
    , m_plater(plater)
{
    SetBackgroundColour(*wxWHITE); // make sure background color set for dialog
    SetForegroundColour(wxColour("#363636"));
    SetFont(Label::Body_14);

    wxBoxSizer* v_sizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(v_sizer);

    // Settings
    wxString start_str    = _L("Start speed: ");
    wxString end_vol_str  = _L("End speed: ");
    wxString vol_step_str = _L("Step") + ": ";
    int text_max = GetTextMax(this, std::vector<wxString>{start_str, end_vol_str, vol_step_str});

    auto st_size = FromDIP(wxSize(text_max, -1));
    auto ti_size = FromDIP(wxSize(120, -1));

    LabeledStaticBox* stb = new LabeledStaticBox(this, _L("Settings"));
    wxStaticBoxSizer* settings_sizer = new wxStaticBoxSizer(stb, wxVERTICAL);

    settings_sizer->AddSpacer(FromDIP(5));

    // start vol
    auto start_vol_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto start_vol_text = new wxStaticText(this, wxID_ANY, start_str, wxDefaultPosition, st_size, wxALIGN_LEFT);
    m_tiStart = new TextInput(this, std::to_string(40), "mm/s", "", wxDefaultPosition, ti_size);
    m_tiStart->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));

    start_vol_sizer->Add(start_vol_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    start_vol_sizer->Add(m_tiStart     , 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    settings_sizer->Add(start_vol_sizer, 0, wxLEFT, FromDIP(3));

    // end vol
    auto end_vol_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto end_vol_text = new wxStaticText(this, wxID_ANY, end_vol_str, wxDefaultPosition, st_size, wxALIGN_LEFT);
    m_tiEnd = new TextInput(this, std::to_string(200), "mm/s", "", wxDefaultPosition, ti_size);
    m_tiStart->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
    end_vol_sizer->Add(end_vol_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    end_vol_sizer->Add(m_tiEnd     , 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    settings_sizer->Add(end_vol_sizer, 0, wxLEFT, FromDIP(3));

    // vol step
    auto vol_step_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto vol_step_text = new wxStaticText(this, wxID_ANY, vol_step_str, wxDefaultPosition, st_size, wxALIGN_LEFT);
    m_tiStep = new TextInput(this, wxString::FromDouble(10), "mm/s", "", wxDefaultPosition, ti_size);
    m_tiStart->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
    vol_step_sizer->Add(vol_step_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    vol_step_sizer->Add(m_tiStep     , 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    settings_sizer->Add(vol_step_sizer, 0, wxLEFT, FromDIP(3));

    settings_sizer->AddSpacer(FromDIP(5));

    v_sizer->Add(settings_sizer, 0, wxTOP | wxRIGHT | wxLEFT | wxEXPAND, FromDIP(10));
    v_sizer->AddSpacer(FromDIP(5));

    auto dlg_btns = new DialogButtons(this, {"OK"});
    v_sizer->Add(dlg_btns , 0, wxEXPAND);

    dlg_btns->GetOK()->Bind(wxEVT_BUTTON, &VFA_Test_Dlg::on_start, this);

    wxGetApp().UpdateDlgDarkUI(this);

    Layout();
    Fit();
}

VFA_Test_Dlg::~VFA_Test_Dlg()
{
    // Disconnect Events
}

void VFA_Test_Dlg::on_start(wxCommandEvent& event)
{
    bool read_double = false;
    read_double = m_tiStart->GetTextCtrl()->GetValue().ToDouble(&m_params.start);
    read_double = read_double && m_tiEnd->GetTextCtrl()->GetValue().ToDouble(&m_params.end);
    read_double = read_double && m_tiStep->GetTextCtrl()->GetValue().ToDouble(&m_params.step);

    if (!read_double || m_params.start <= 10 || m_params.step <= 0 || m_params.end < (m_params.start + m_params.step)) {
        MessageDialog msg_dlg(nullptr, _L("Please input valid values:\nstart > 10\nstep >= 0\nend > start + step"), wxEmptyString, wxICON_WARNING | wxOK);
        msg_dlg.ShowModal();
        return;
    }

    m_params.mode = CalibMode::Calib_VFA_Tower;
    m_plater->calib_VFA(m_params);
    EndModal(wxID_OK);
}

void VFA_Test_Dlg::on_dpi_changed(const wxRect& suggested_rect)
{
    this->Refresh();
    Fit();
}



// Retraction_Test_Dlg
//

Retraction_Test_Dlg::Retraction_Test_Dlg(wxWindow* parent, wxWindowID id, Plater* plater)
    : DPIDialog(parent, id, _L("Retraction test"), wxDefaultPosition, parent->FromDIP(wxSize(-1, 280)), wxDEFAULT_DIALOG_STYLE), m_plater(plater)
{
    SetBackgroundColour(*wxWHITE); // make sure background color set for dialog
    SetForegroundColour(wxColour("#363636"));
    SetFont(Label::Body_14);

    wxBoxSizer* v_sizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(v_sizer);

    // Settings
    wxString start_length_str = _L("Start retraction length: ");
    wxString end_length_str   = _L("End retraction length: ");
    wxString length_step_str  = _L("Step") + ": ";
    int text_max = GetTextMax(this, std::vector<wxString>{start_length_str, end_length_str, length_step_str});

    auto st_size = FromDIP(wxSize(text_max, -1));
    auto ti_size = FromDIP(wxSize(120, -1));

    LabeledStaticBox* stb = new LabeledStaticBox(this, _L("Settings"));
    wxStaticBoxSizer* settings_sizer = new wxStaticBoxSizer(stb, wxVERTICAL);

    settings_sizer->AddSpacer(FromDIP(5));

    // start length
    auto start_length_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto start_length_text = new wxStaticText(this, wxID_ANY, start_length_str, wxDefaultPosition, st_size, wxALIGN_LEFT);
    m_tiStart = new TextInput(this, std::to_string(0), "mm", "", wxDefaultPosition, ti_size);
    m_tiStart->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));

    start_length_sizer->Add(start_length_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    start_length_sizer->Add(m_tiStart        , 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    settings_sizer->Add(start_length_sizer, 0, wxLEFT, FromDIP(3));

    // end length
    auto end_length_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto end_length_text = new wxStaticText(this, wxID_ANY, end_length_str, wxDefaultPosition, st_size, wxALIGN_LEFT);
    m_tiEnd = new TextInput(this, std::to_string(2), "mm", "", wxDefaultPosition, ti_size);
    m_tiStart->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
    end_length_sizer->Add(end_length_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    end_length_sizer->Add(m_tiEnd        , 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    settings_sizer->Add(end_length_sizer, 0, wxLEFT, FromDIP(3));

    // length step
    auto length_step_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto length_step_text = new wxStaticText(this, wxID_ANY, length_step_str, wxDefaultPosition, st_size, wxALIGN_LEFT);
    m_tiStep = new TextInput(this, wxString::FromDouble(0.1), "mm", "", wxDefaultPosition, ti_size);
    m_tiStart->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
    length_step_sizer->Add(length_step_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    length_step_sizer->Add(m_tiStep        , 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    settings_sizer->Add(length_step_sizer, 0, wxLEFT, FromDIP(3));

    settings_sizer->AddSpacer(FromDIP(5));

    v_sizer->Add(settings_sizer, 0, wxTOP | wxRIGHT | wxLEFT | wxEXPAND, FromDIP(10));
    v_sizer->AddSpacer(FromDIP(5));

    auto dlg_btns = new DialogButtons(this, {"OK"});
    v_sizer->Add(dlg_btns , 0, wxEXPAND);

    dlg_btns->GetOK()->Bind(wxEVT_BUTTON, &Retraction_Test_Dlg::on_start, this);

    wxGetApp().UpdateDlgDarkUI(this);

    Layout();
    Fit();
}

Retraction_Test_Dlg::~Retraction_Test_Dlg() {
    // Disconnect Events
}

void Retraction_Test_Dlg::on_start(wxCommandEvent& event) {
    bool read_double = false;
    read_double = m_tiStart->GetTextCtrl()->GetValue().ToDouble(&m_params.start);
    read_double = read_double && m_tiEnd->GetTextCtrl()->GetValue().ToDouble(&m_params.end);
    read_double = read_double && m_tiStep->GetTextCtrl()->GetValue().ToDouble(&m_params.step);

    if (!read_double || m_params.start < 0 || m_params.step <= 0 || m_params.end < (m_params.start + m_params.step)) {
        MessageDialog msg_dlg(nullptr, _L("Please input valid values:\nstart > 0\nstep >= 0\nend > start + step"), wxEmptyString, wxICON_WARNING | wxOK);
        msg_dlg.ShowModal();
        return;
    }

    m_params.mode = CalibMode::Calib_Retraction_tower;
    m_plater->calib_retraction(m_params);
    EndModal(wxID_OK);

}

void Retraction_Test_Dlg::on_dpi_changed(const wxRect& suggested_rect) {
    this->Refresh();
    Fit();

}

// Input_Shaping_Freq_Test_Dlg
//

Input_Shaping_Freq_Test_Dlg::Input_Shaping_Freq_Test_Dlg(wxWindow* parent, wxWindowID id, Plater* plater)
    : DPIDialog(parent, id, _L("Input shaping Frequency test"), wxDefaultPosition, parent->FromDIP(wxSize(-1, 280)), wxDEFAULT_DIALOG_STYLE), m_plater(plater)
{
    SetBackgroundColour(*wxWHITE); // make sure background color set for dialog
    SetForegroundColour(wxColour("#363636"));
    SetFont(Label::Body_14);

    wxBoxSizer* v_sizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(v_sizer);

    // Model selection
    auto labeled_box_model = new LabeledStaticBox(this, _L("Test model"));
    auto model_box = new wxStaticBoxSizer(labeled_box_model, wxHORIZONTAL);

    m_rbModel = new RadioGroup(this, { _L("Ringing Tower"), _L("Fast Tower") }, wxHORIZONTAL);
    model_box->Add(m_rbModel, 0, wxALL | wxEXPAND, FromDIP(4));
    v_sizer->Add(model_box, 0, wxTOP | wxRIGHT | wxLEFT | wxEXPAND, FromDIP(10));

    // Settings
    wxString x_axis_str = "X " + _L("Start / End") + ": ";
    wxString y_axis_str = "Y " + _L("Start / End") + ": ";
    int text_max = GetTextMax(this, std::vector<wxString>{x_axis_str, y_axis_str});

    auto st_size = FromDIP(wxSize(text_max, -1));
    auto ti_size = FromDIP(wxSize(120, -1));

    LabeledStaticBox* stb = new LabeledStaticBox(this, _L("Frequency settings"));
    wxStaticBoxSizer* settings_sizer = new wxStaticBoxSizer(stb, wxVERTICAL);

    settings_sizer->AddSpacer(FromDIP(5));

    // X axis frequencies
    auto x_freq_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto start_x_text = new wxStaticText(this, wxID_ANY, x_axis_str, wxDefaultPosition, st_size, wxALIGN_LEFT);
    m_tiFreqStartX = new TextInput(this, std::to_string(15) , "Hz", "", wxDefaultPosition, ti_size);
    m_tiFreqStartX->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
    m_tiFreqEndX   = new TextInput(this, std::to_string(110), "Hz", "", wxDefaultPosition, ti_size);
    m_tiFreqEndX->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
    
    x_freq_sizer->Add(start_x_text  , 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    x_freq_sizer->Add(m_tiFreqStartX, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    x_freq_sizer->Add(m_tiFreqEndX  , 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    settings_sizer->Add(x_freq_sizer, 0, wxLEFT, FromDIP(3));

    // Y axis frequencies
    auto y_freq_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto start_y_text = new wxStaticText(this, wxID_ANY, y_axis_str, wxDefaultPosition, st_size, wxALIGN_LEFT);
    m_tiFreqStartY = new TextInput(this, std::to_string(15) , "Hz", "", wxDefaultPosition, ti_size);
    m_tiFreqStartY->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
    m_tiFreqEndY =   new TextInput(this, std::to_string(110), "Hz", "", wxDefaultPosition, ti_size);
    m_tiFreqEndY->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
    
    y_freq_sizer->Add(start_y_text  , 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    y_freq_sizer->Add(m_tiFreqStartY, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    y_freq_sizer->Add(m_tiFreqEndY  , 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    settings_sizer->Add(y_freq_sizer, 0, wxLEFT, FromDIP(3));

    // Damping Factor
    wxString damping_factor_str = _L("Damp: ");
    auto damping_factor_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto damping_factor_text = new wxStaticText(this, wxID_ANY, damping_factor_str, wxDefaultPosition, st_size, wxALIGN_LEFT);
    m_tiDampingFactor = new TextInput(this, wxString::Format("%.3f", 0.15), "", "", wxDefaultPosition, ti_size);
    m_tiDampingFactor->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
    
    damping_factor_sizer->Add(damping_factor_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    damping_factor_sizer->Add(m_tiDampingFactor  , 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    settings_sizer->Add(damping_factor_sizer, 0, wxLEFT, FromDIP(3));
    
    settings_sizer->AddSpacer(FromDIP(5));

    // Add a note explaining that 0 means use default value
    auto note_text = new wxStaticText(this, wxID_ANY, _L("Recommended: Set Damp to 0.\nThis will use the printer's default or the last saved value."), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    note_text->SetForegroundColour(wxColour(128, 128, 128));
    settings_sizer->Add(note_text, 0, wxALL, FromDIP(5));

    settings_sizer->AddSpacer(FromDIP(5));

    v_sizer->Add(settings_sizer, 0, wxTOP | wxRIGHT | wxLEFT | wxEXPAND, FromDIP(10));
    v_sizer->AddSpacer(FromDIP(5));

    auto dlg_btns = new DialogButtons(this, {"OK"});
    v_sizer->Add(dlg_btns , 0, wxEXPAND);

    dlg_btns->GetOK()->Bind(wxEVT_BUTTON, &Input_Shaping_Freq_Test_Dlg::on_start, this);

    wxGetApp().UpdateDlgDarkUI(this);

    Layout();
    Fit();
}

Input_Shaping_Freq_Test_Dlg::~Input_Shaping_Freq_Test_Dlg() {
    // Disconnect Events
}

void Input_Shaping_Freq_Test_Dlg::on_start(wxCommandEvent& event) {
    bool read_double = false;
    read_double = m_tiFreqStartX->GetTextCtrl()->GetValue().ToDouble(&m_params.freqStartX);
    read_double = read_double && m_tiFreqEndX->GetTextCtrl()->GetValue().ToDouble(&m_params.freqEndX);
    read_double = read_double && m_tiFreqStartY->GetTextCtrl()->GetValue().ToDouble(&m_params.freqStartY);
    read_double = read_double && m_tiFreqEndY->GetTextCtrl()->GetValue().ToDouble(&m_params.freqEndY);
    read_double = read_double && m_tiDampingFactor->GetTextCtrl()->GetValue().ToDouble(&m_params.start);

    if (!read_double ||
        m_params.freqStartX < 0 || m_params.freqEndX > 500 ||
        m_params.freqStartY < 0 || m_params.freqEndX > 500 ||
        m_params.freqStartX >= m_params.freqEndX ||
        m_params.freqStartY >= m_params.freqEndY) {
        MessageDialog msg_dlg(nullptr, _L("Please input valid values:\n(0 < FreqStart < FreqEnd < 500)"), wxEmptyString, wxICON_WARNING | wxOK);
        msg_dlg.ShowModal();
        return;
    }

    if (m_params.start < 0 || m_params.start >= 1) {
        MessageDialog msg_dlg(nullptr, _L("Please input a valid damping factor (0 < Damping/zeta factor <= 1)"), wxEmptyString, wxICON_WARNING | wxOK);
        msg_dlg.ShowModal();
        return;
    }

    m_params.mode = CalibMode::Calib_Input_shaping_freq;
    
    // Set model type based on selection
    m_params.test_model = m_rbModel->GetSelection() == 0 ? 0 : 1; // 0 = Ringing Tower, 1 = Fast Tower
    
    m_plater->calib_input_shaping_freq(m_params);
    EndModal(wxID_OK);
}

void Input_Shaping_Freq_Test_Dlg::on_dpi_changed(const wxRect& suggested_rect) {
    this->Refresh();
    Fit();
}

// Input_Shaping_Damp_Test_Dlg
//

Input_Shaping_Damp_Test_Dlg::Input_Shaping_Damp_Test_Dlg(wxWindow* parent, wxWindowID id, Plater* plater)
    : DPIDialog(parent, id, _L("Input shaping Damp test"), wxDefaultPosition, parent->FromDIP(wxSize(-1, 280)), wxDEFAULT_DIALOG_STYLE), m_plater(plater)
{
    SetBackgroundColour(*wxWHITE); // make sure background color set for dialog
    SetForegroundColour(wxColour("#363636"));
    SetFont(Label::Body_14);

    wxBoxSizer* v_sizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(v_sizer);

    // Model selection
    auto labeled_box_model = new LabeledStaticBox(this, _L("Test model"));
    auto model_box = new wxStaticBoxSizer(labeled_box_model, wxHORIZONTAL);

    m_rbModel = new RadioGroup(this, { _L("Ringing Tower"), _L("Fast Tower") }, wxHORIZONTAL);
    model_box->Add(m_rbModel, 0, wxALL | wxEXPAND, FromDIP(4));
    v_sizer->Add(model_box, 0, wxTOP | wxRIGHT | wxLEFT | wxEXPAND, FromDIP(10));

    // Settings
    wxString freq_str = _L("Frequency") + " X / Y: ";
    wxString damp_str = _L("Damp") + " " + _L("Start / End") + ": ";
    int text_max = GetTextMax(this, std::vector<wxString>{freq_str, damp_str});

    auto st_size = FromDIP(wxSize(text_max, -1));
    auto ti_size = FromDIP(wxSize(120, -1));

    LabeledStaticBox* stb = new LabeledStaticBox(this, _L("Frequency settings"));
    wxStaticBoxSizer* settings_sizer = new wxStaticBoxSizer(stb, wxVERTICAL);

    settings_sizer->AddSpacer(FromDIP(5));

    auto freq_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto freq_text = new wxStaticText(this, wxID_ANY, freq_str, wxDefaultPosition, st_size, wxALIGN_LEFT);
    m_tiFreqX = new TextInput(this, std::to_string(30), "Hz", "", wxDefaultPosition, ti_size);
    m_tiFreqX->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
    m_tiFreqY = new TextInput(this, std::to_string(30), "Hz", "", wxDefaultPosition, ti_size);
    m_tiFreqY->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
    freq_sizer->Add(freq_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    freq_sizer->Add(m_tiFreqX, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    freq_sizer->Add(m_tiFreqY, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    settings_sizer->Add(freq_sizer, 0, wxLEFT, FromDIP(3));
    
    // Damping Factor Start and End
    auto damp_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto damp_text = new wxStaticText(this, wxID_ANY, damp_str, wxDefaultPosition, st_size, wxALIGN_LEFT);
    m_tiDampingFactorStart = new TextInput(this, wxString::Format("%.3f", 0.00), "", "", wxDefaultPosition, ti_size);
    m_tiDampingFactorStart->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
    m_tiDampingFactorEnd   = new TextInput(this, wxString::Format("%.3f", 0.40), "", "", wxDefaultPosition, ti_size);
    m_tiDampingFactorEnd->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
    damp_sizer->Add(damp_text             , 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    damp_sizer->Add(m_tiDampingFactorStart, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    damp_sizer->Add(m_tiDampingFactorEnd  , 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    settings_sizer->Add(damp_sizer, 0, wxLEFT, FromDIP(3));

    settings_sizer->AddSpacer(FromDIP(5));

    // Add a note to explain users to use their previously calculated frequency
    auto note_text = new wxStaticText(this, wxID_ANY, _L("Note: Use previously calculated frequencies."), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    note_text->SetForegroundColour(wxColour(128, 128, 128));
    settings_sizer->Add(note_text, 0, wxALL, FromDIP(5));

    settings_sizer->AddSpacer(FromDIP(5));

    v_sizer->Add(settings_sizer, 0, wxTOP | wxRIGHT | wxLEFT | wxEXPAND, FromDIP(10));
    v_sizer->AddSpacer(FromDIP(5));

    auto dlg_btns = new DialogButtons(this, {"OK"});
    v_sizer->Add(dlg_btns , 0, wxEXPAND);

    dlg_btns->GetOK()->Bind(wxEVT_BUTTON, &Input_Shaping_Damp_Test_Dlg::on_start, this);

    wxGetApp().UpdateDlgDarkUI(this);

    Layout();
    Fit();
}

Input_Shaping_Damp_Test_Dlg::~Input_Shaping_Damp_Test_Dlg() {
    // Disconnect Events
}

void Input_Shaping_Damp_Test_Dlg::on_start(wxCommandEvent& event) {
    bool read_double = false;
    read_double = m_tiFreqX->GetTextCtrl()->GetValue().ToDouble(&m_params.freqStartX);
    read_double = read_double && m_tiFreqY->GetTextCtrl()->GetValue().ToDouble(&m_params.freqStartY);
    read_double = read_double && m_tiDampingFactorStart->GetTextCtrl()->GetValue().ToDouble(&m_params.start);
    read_double = read_double && m_tiDampingFactorEnd->GetTextCtrl()->GetValue().ToDouble(&m_params.end);

    if (!read_double ||
        m_params.freqStartX < 0 || m_params.freqStartX > 500 ||
        m_params.freqStartY < 0 || m_params.freqStartY > 500 ) {
        MessageDialog msg_dlg(nullptr, _L("Please input valid values:\n(0 < Freq < 500)"), wxEmptyString, wxICON_WARNING | wxOK);
        msg_dlg.ShowModal();
        return;
    }

    if (m_params.start < 0 || m_params.end > 1
        || m_params.start >= m_params.end) {
        MessageDialog msg_dlg(nullptr, _L("Please input a valid damping factor (0 <= DampingStart < DampingEnd <= 1)"), wxEmptyString, wxICON_WARNING | wxOK);
        msg_dlg.ShowModal();
        return;
    }

    m_params.mode = CalibMode::Calib_Input_shaping_damp;
    
    // Set model type based on selection
    m_params.test_model = m_rbModel->GetSelection() == 0 ? 0 : 1; // 0 = Ringing Tower, 1 = Fast Tower
    
    m_plater->calib_input_shaping_damp(m_params);
    EndModal(wxID_OK);
}

void Input_Shaping_Damp_Test_Dlg::on_dpi_changed(const wxRect& suggested_rect) {
    this->Refresh();
    Fit();
}

// Junction_Deviation_Test_Dlg
//

Junction_Deviation_Test_Dlg::Junction_Deviation_Test_Dlg(wxWindow* parent, wxWindowID id, Plater* plater)
    : DPIDialog(parent, id, _L("Junction Deviation test"), wxDefaultPosition, parent->FromDIP(wxSize(-1, 280)), wxDEFAULT_DIALOG_STYLE), m_plater(plater)
{
    SetBackgroundColour(*wxWHITE); // make sure background color set for dialog
    SetForegroundColour(wxColour("#363636"));
    SetFont(Label::Body_14);

    wxBoxSizer* v_sizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(v_sizer);

    // Model selection
    auto labeled_box_model = new LabeledStaticBox(this, _L("Test model"));
    auto model_box = new wxStaticBoxSizer(labeled_box_model, wxHORIZONTAL);

    m_rbModel = new RadioGroup(this, { _L("Ringing Tower"), _L("Fast Tower") }, wxHORIZONTAL);
    model_box->Add(m_rbModel, 0, wxALL | wxEXPAND, FromDIP(4));
    v_sizer->Add(model_box, 0, wxTOP | wxRIGHT | wxLEFT | wxEXPAND, FromDIP(10));

    // Settings
    wxString start_jd_str = _L("Start junction deviation: ");
    wxString end_jd_str   = _L("End junction deviation: ");
    int text_max = GetTextMax(this, std::vector<wxString>{start_jd_str, end_jd_str});

    auto st_size = FromDIP(wxSize(text_max, -1));
    auto ti_size = FromDIP(wxSize(120, -1));

    LabeledStaticBox* stb = new LabeledStaticBox(this, _L("Junction Deviation settings"));
    wxStaticBoxSizer* settings_sizer = new wxStaticBoxSizer(stb, wxVERTICAL);

    settings_sizer->AddSpacer(FromDIP(5));

    // Start junction deviation
    auto start_jd_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto start_jd_text = new wxStaticText(this, wxID_ANY, start_jd_str, wxDefaultPosition, st_size, wxALIGN_LEFT);
    m_tiJDStart = new TextInput(this, wxString::Format("%.3f", 0.000), "mm", "", wxDefaultPosition, ti_size);
    m_tiJDStart->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
    start_jd_sizer->Add(start_jd_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    start_jd_sizer->Add(m_tiJDStart  , 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    settings_sizer->Add(start_jd_sizer, 0, wxLEFT, FromDIP(3));

    // End junction deviation
    auto end_jd_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto end_jd_text = new wxStaticText(this, wxID_ANY, end_jd_str, wxDefaultPosition, st_size, wxALIGN_LEFT);
    m_tiJDEnd = new TextInput(this, wxString::Format("%.3f", 0.250), "mm", "", wxDefaultPosition, ti_size);
    m_tiJDEnd->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
    end_jd_sizer->Add(end_jd_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    end_jd_sizer->Add(m_tiJDEnd  , 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(2));
    settings_sizer->Add(end_jd_sizer, 0, wxLEFT, FromDIP(3));

    settings_sizer->AddSpacer(FromDIP(5));

    // Add note about junction deviation
    auto note_text = new wxStaticText(this, wxID_ANY, _L("Note: Lower values = sharper corners but slower speeds"), 
                                    wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    note_text->SetForegroundColour(wxColour(128, 128, 128));
    settings_sizer->Add(note_text, 0, wxALL, FromDIP(5));

    v_sizer->Add(settings_sizer, 0, wxTOP | wxRIGHT | wxLEFT | wxEXPAND, FromDIP(10));
    v_sizer->AddSpacer(FromDIP(5));

    auto dlg_btns = new DialogButtons(this, {"OK"});
    v_sizer->Add(dlg_btns , 0, wxEXPAND);

    dlg_btns->GetOK()->Bind(wxEVT_BUTTON, &Junction_Deviation_Test_Dlg::on_start, this);

    wxGetApp().UpdateDlgDarkUI(this);

    Layout();
    Fit();
}

Junction_Deviation_Test_Dlg::~Junction_Deviation_Test_Dlg() {
    // Disconnect Events
}

void Junction_Deviation_Test_Dlg::on_start(wxCommandEvent& event) {
    bool read_double = false;
    read_double = m_tiJDStart->GetTextCtrl()->GetValue().ToDouble(&m_params.start);
    read_double = read_double && m_tiJDEnd->GetTextCtrl()->GetValue().ToDouble(&m_params.end);

    if (!read_double || m_params.start < 0 || m_params.end >= 1 || m_params.start >= m_params.end) {
        MessageDialog msg_dlg(nullptr, _L("Please input valid values:\n(0 <= Junction Deviation < 1)"), wxEmptyString, wxICON_WARNING | wxOK);
        msg_dlg.ShowModal();
        return;
    } else if (m_params.end > 0.3) {
        MessageDialog msg_dlg(nullptr, _L("NOTE: High values may cause Layer shift"), wxEmptyString, wxICON_WARNING | wxOK);
        msg_dlg.ShowModal();
    }

    m_params.mode = CalibMode::Calib_Junction_Deviation;
    
    // Set model type based on selection
    m_params.test_model = m_rbModel->GetSelection() == 0 ? 0 : 1; // 0 = Ringing Tower, 1 = Fast Tower
    
    m_plater->calib_junction_deviation(m_params);
    EndModal(wxID_OK);
}

void Junction_Deviation_Test_Dlg::on_dpi_changed(const wxRect& suggested_rect) {
    this->Refresh();
    Fit();
}

namespace {

wxString pane_factor_label(PaneCalibrationFactor factor)
{
    switch (factor) {
    case PaneCalibrationFactor::NozzleTemperature: return _L("Nozzle temperature (°C)");
    case PaneCalibrationFactor::PrintSpeed: return _L("Print speed (mm/s)");
    case PaneCalibrationFactor::FlowRatio: return _L("Flow ratio");
    case PaneCalibrationFactor::LayerHeight: return _L("Layer height (mm)");
    case PaneCalibrationFactor::MaxFanSpeed: return _L("Part cooling fan (%)");
    case PaneCalibrationFactor::WallFanSpeed: return _L("Wall fan (%)");
    case PaneCalibrationFactor::IroningFanSpeed: return _L("Ironing fan (%)");
    case PaneCalibrationFactor::AuxiliaryFanSpeed: return _L("Auxiliary fan (%)");
    case PaneCalibrationFactor::IroningType: return _L("Ironing mode");
    case PaneCalibrationFactor::IroningFlow: return _L("Ironing flow (%)");
    case PaneCalibrationFactor::IroningAngle: return _L("Ironing direction");
    case PaneCalibrationFactor::LineWidth: return _L("Line width (mm)");
    case PaneCalibrationFactor::IroningSpeed: return _L("Ironing speed (mm/s)");
    case PaneCalibrationFactor::IroningSpacing: return _L("Ironing spacing (mm)");
    }
    return _L("Unknown factor");
}

bool pane_factor_is_categorical(PaneCalibrationFactor factor)
{
    return factor == PaneCalibrationFactor::IroningType || factor == PaneCalibrationFactor::IroningAngle;
}

} // namespace

Pane_Calibration_Dlg::Pane_Calibration_Dlg(wxWindow *parent, wxWindowID id, Plater *plater, PaneCalibrationTool tool)
    : DPIDialog(parent, id,
                tool == PaneCalibrationTool::ClearFilament ? _L("Clear filament calibration") : _L("Ironing calibration"),
                wxDefaultPosition, parent->FromDIP(wxSize(760, 760)), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_plater(plater), m_tool(tool)
{
    SetBackgroundColour(*wxWHITE);
    SetForegroundColour(wxColour("#363636"));
    SetFont(Label::Body_14);

    const PaneCalibrationConfig defaults = default_pane_calibration_config(tool);
    auto *outer = new wxBoxSizer(wxVERTICAL);
    SetSizer(outer);
    auto *scroll = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
    scroll->SetScrollRate(0, FromDIP(12));
    auto *content = new wxBoxSizer(wxVERTICAL);
    scroll->SetSizer(content);

    auto *design_box = new wxStaticBoxSizer(wxVERTICAL, scroll, _L("Experiment design"));
    auto *design_row = new wxBoxSizer(wxHORIZONTAL);
    design_row->Add(new wxStaticText(scroll, wxID_ANY, _L("Design")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    m_design = new wxChoice(scroll, wxID_ANY);
    m_design->Append(_L("Linear series"));
    m_design->Append(_L("2D grid"));
    m_design->Append(_L("Taguchi orthogonal array"));
    m_design->SetSelection(2);
    design_row->Add(m_design, 1, wxRIGHT, FromDIP(16));
    design_row->Add(new wxStaticText(scroll, wxID_ANY, _L("Taguchi levels")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    m_taguchi_levels = new wxSpinCtrl(scroll, wxID_ANY);
    m_taguchi_levels->SetRange(2, 4);
    m_taguchi_levels->SetValue(4);
    design_row->Add(m_taguchi_levels, 0);
    design_box->Add(design_row, 0, wxALL | wxEXPAND, FromDIP(6));
    content->Add(design_box, 0, wxALL | wxEXPAND, FromDIP(8));

    auto *factors_box = new wxStaticBoxSizer(wxVERTICAL, scroll, _L("Factors"));
    auto *factor_grid = new wxFlexGridSizer(5, FromDIP(5), FromDIP(8));
    factor_grid->AddGrowableCol(1, 1);
    factor_grid->Add(new wxStaticText(scroll, wxID_ANY, _L("Use")));
    factor_grid->Add(new wxStaticText(scroll, wxID_ANY, _L("Factor")));
    factor_grid->Add(new wxStaticText(scroll, wxID_ANY, _L("Minimum")));
    factor_grid->Add(new wxStaticText(scroll, wxID_ANY, _L("Maximum")));
    factor_grid->Add(new wxStaticText(scroll, wxID_ANY, _L("Levels")));
    for (const PaneCalibrationFactorSetting &factor : defaults.factors) {
        FactorControls controls;
        controls.factor = factor.factor;
        controls.enabled = new wxCheckBox(scroll, wxID_ANY, wxEmptyString);
        controls.enabled->SetValue(factor.enabled);
        factor_grid->Add(controls.enabled, 0, wxALIGN_CENTER);
        factor_grid->Add(new wxStaticText(scroll, wxID_ANY, pane_factor_label(factor.factor)), 0, wxALIGN_CENTER_VERTICAL);

        controls.minimum = new wxSpinCtrlDouble(scroll, wxID_ANY);
        controls.maximum = new wxSpinCtrlDouble(scroll, wxID_ANY);
        const bool percent = factor.factor == PaneCalibrationFactor::MaxFanSpeed || factor.factor == PaneCalibrationFactor::WallFanSpeed ||
                             factor.factor == PaneCalibrationFactor::IroningFanSpeed ||
                             factor.factor == PaneCalibrationFactor::AuxiliaryFanSpeed || factor.factor == PaneCalibrationFactor::IroningFlow;
        const double lower = factor.factor == PaneCalibrationFactor::NozzleTemperature ? 100. :
                             factor.factor == PaneCalibrationFactor::FlowRatio ? 0.5 : 0.;
        const double upper = factor.factor == PaneCalibrationFactor::NozzleTemperature ? 500. :
                             factor.factor == PaneCalibrationFactor::FlowRatio ? 1.5 :
                             percent ? 100. : 500.;
        const int digits = factor.factor == PaneCalibrationFactor::NozzleTemperature || percent ? 0 : 3;
        for (wxSpinCtrlDouble *spin : {controls.minimum, controls.maximum}) {
            spin->SetRange(lower, upper);
            spin->SetDigits(digits);
            spin->SetIncrement(digits == 0 ? 1. : 0.01);
            spin->SetMinSize(FromDIP(wxSize(95, -1)));
        }
        controls.minimum->SetValue(factor.minimum);
        controls.maximum->SetValue(factor.maximum);
        const bool categorical = pane_factor_is_categorical(factor.factor);
        controls.minimum->Enable(!categorical);
        controls.maximum->Enable(!categorical);
        factor_grid->Add(controls.minimum, 0, wxEXPAND);
        factor_grid->Add(controls.maximum, 0, wxEXPAND);
        controls.levels = new wxSpinCtrl(scroll, wxID_ANY);
        controls.levels->SetRange(2, categorical ? 4 : 10);
        controls.levels->SetValue(int(factor.levels));
        factor_grid->Add(controls.levels, 0, wxEXPAND);
        m_factors.emplace_back(controls);
    }
    factors_box->Add(factor_grid, 1, wxALL | wxEXPAND, FromDIP(6));
    content->Add(factors_box, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, FromDIP(8));

    auto make_dimension = [scroll](double value, double minimum, double maximum, int digits) {
        auto *spin = new wxSpinCtrlDouble(scroll, wxID_ANY);
        spin->SetRange(minimum, maximum);
        spin->SetDigits(digits);
        spin->SetIncrement(digits == 0 ? 1. : 0.1);
        spin->SetValue(value);
        return spin;
    };
    auto *geometry_box = new wxStaticBoxSizer(wxVERTICAL, scroll, _L("Pane and label geometry"));
    auto *geometry_grid = new wxFlexGridSizer(4, FromDIP(5), FromDIP(8));
    geometry_grid->AddGrowableCol(1, 1);
    geometry_grid->AddGrowableCol(3, 1);
    auto add_geometry = [&](const wxString &label, wxSpinCtrlDouble *control) {
        geometry_grid->Add(new wxStaticText(scroll, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
        geometry_grid->Add(control, 1, wxEXPAND);
    };
    m_pane_width = make_dimension(defaults.pane_width, 5., 300., 1);
    m_pane_depth = make_dimension(defaults.pane_depth, 5., 300., 1);
    m_pane_height = make_dimension(defaults.pane_height, 0.2, 50., 2);
    m_pane_gap = make_dimension(defaults.pane_gap, 0., 50., 1);
    add_geometry(_L("Width (mm)"), m_pane_width);
    add_geometry(_L("Depth (mm)"), m_pane_depth);
    add_geometry(_L("Height (mm)"), m_pane_height);
    add_geometry(_L("Gap (mm)"), m_pane_gap);
    m_glyph_height = make_dimension(defaults.label_glyph_height, 1., 20., 1);
    m_label_relief = make_dimension(defaults.label_relief, 0.1, 10., 1);
    add_geometry(_L("Glyph height (mm)"), m_glyph_height);
    add_geometry(_L("Label relief (mm)"), m_label_relief);
    geometry_box->Add(geometry_grid, 0, wxALL | wxEXPAND, FromDIP(6));

    auto *option_row = new wxBoxSizer(wxHORIZONTAL);
    m_mouse_ears = new wxCheckBox(scroll, wxID_ANY, _L("Mouse ears"));
    m_labels = new wxCheckBox(scroll, wxID_ANY, _L("Emboss labels"));
    option_row->Add(m_mouse_ears, 0, wxRIGHT, FromDIP(18));
    option_row->Add(m_labels, 0);
    geometry_box->Add(option_row, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(6));

    const DynamicPrintConfig full_config = wxGetApp().preset_bundle->full_config();
    const auto *nozzle_diameters = full_config.opt<ConfigOptionFloats>("nozzle_diameter");
    const int extruder_count = std::max<int>(1, nozzle_diameters == nullptr ? 1 : int(nozzle_diameters->values.size()));
    auto *extruder_row = new wxBoxSizer(wxHORIZONTAL);
    extruder_row->Add(new wxStaticText(scroll, wxID_ANY, _L("Pane extruder")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
    m_pane_extruder = new wxSpinCtrl(scroll, wxID_ANY);
    m_pane_extruder->SetRange(1, extruder_count);
    m_pane_extruder->SetValue(1);
    extruder_row->Add(m_pane_extruder, 0, wxRIGHT, FromDIP(18));
    extruder_row->Add(new wxStaticText(scroll, wxID_ANY, _L("Label extruder")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
    m_label_extruder = new wxSpinCtrl(scroll, wxID_ANY);
    m_label_extruder->SetRange(1, extruder_count);
    m_label_extruder->SetValue(1);
    extruder_row->Add(m_label_extruder, 0);
    geometry_box->Add(extruder_row, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(6));
    content->Add(geometry_box, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, FromDIP(8));

    m_preview = new wxStaticText(scroll, wxID_ANY, wxEmptyString);
    m_preview->Wrap(FromDIP(700));
    content->Add(m_preview, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, FromDIP(12));
    auto *warning = new wxStaticText(scroll, wxID_ANY,
        _L("Regional temperature changes are nonblocking. Small modifier or layer-range regions may finish before the nozzle stabilizes."));
    warning->SetForegroundColour(wxColour(180, 90, 0));
    warning->Wrap(FromDIP(700));
    content->Add(warning, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, FromDIP(12));
    outer->Add(scroll, 1, wxEXPAND);

    auto *buttons = new DialogButtons(this, {"Generate"});
    m_generate = buttons->GetOK();
    m_generate->Bind(wxEVT_BUTTON, &Pane_Calibration_Dlg::on_start, this);
    outer->Add(buttons, 0, wxEXPAND);

    auto refresh = [this](wxCommandEvent &) { refresh_preview(); };
    m_design->Bind(wxEVT_CHOICE, refresh);
    m_taguchi_levels->Bind(wxEVT_SPINCTRL, refresh);
    for (FactorControls &factor : m_factors) {
        factor.enabled->Bind(wxEVT_CHECKBOX, refresh);
        factor.minimum->Bind(wxEVT_SPINCTRLDOUBLE, refresh);
        factor.maximum->Bind(wxEVT_SPINCTRLDOUBLE, refresh);
        factor.levels->Bind(wxEVT_SPINCTRL, refresh);
    }
    for (wxSpinCtrlDouble *control : {m_pane_width, m_pane_depth, m_pane_height, m_pane_gap, m_glyph_height, m_label_relief})
        control->Bind(wxEVT_SPINCTRLDOUBLE, refresh);
    m_mouse_ears->Bind(wxEVT_CHECKBOX, refresh);
    m_labels->Bind(wxEVT_CHECKBOX, refresh);
    m_pane_extruder->Bind(wxEVT_SPINCTRL, refresh);
    m_label_extruder->Bind(wxEVT_SPINCTRL, refresh);

    const std::string section = m_tool == PaneCalibrationTool::ClearFilament ? "clear_filament_calibration" : "ironing_calibration";
    auto load_integer = [&](const std::string &key, int fallback) {
        const std::string value = wxGetApp().app_config->get(section, key);
        try { return value.empty() ? fallback : std::stoi(value); } catch (...) { return fallback; }
    };
    auto load_double = [&](const std::string &key, double fallback) {
        const std::string value = wxGetApp().app_config->get(section, key);
        try { return value.empty() ? fallback : std::stod(value); } catch (...) { return fallback; }
    };
    m_design->SetSelection(std::clamp(load_integer("design", m_design->GetSelection()), 0, 2));
    m_taguchi_levels->SetValue(load_integer("taguchi_levels", m_taguchi_levels->GetValue()));
    for (FactorControls &factor : m_factors) {
        const std::string prefix = pane_calibration_factor_key(factor.factor) + "_";
        factor.enabled->SetValue(load_integer(prefix + "enabled", factor.enabled->GetValue()) != 0);
        if (!pane_factor_is_categorical(factor.factor)) {
            factor.minimum->SetValue(load_double(prefix + "minimum", factor.minimum->GetValue()));
            factor.maximum->SetValue(load_double(prefix + "maximum", factor.maximum->GetValue()));
        }
        factor.levels->SetValue(load_integer(prefix + "levels", factor.levels->GetValue()));
    }
    m_pane_width->SetValue(load_double("pane_width", m_pane_width->GetValue()));
    m_pane_depth->SetValue(load_double("pane_depth", m_pane_depth->GetValue()));
    m_pane_height->SetValue(load_double("pane_height", m_pane_height->GetValue()));
    m_pane_gap->SetValue(load_double("pane_gap", m_pane_gap->GetValue()));
    m_mouse_ears->SetValue(load_integer("mouse_ears", 0) != 0);
    m_labels->SetValue(load_integer("labels", 0) != 0);
    m_glyph_height->SetValue(load_double("glyph_height", m_glyph_height->GetValue()));
    m_label_relief->SetValue(load_double("label_relief", m_label_relief->GetValue()));
    m_pane_extruder->SetValue(load_integer("pane_extruder", 1));
    m_label_extruder->SetValue(load_integer("label_extruder", 1));

    wxGetApp().UpdateDlgDarkUI(this);
    refresh_preview();
    CentreOnParent();
}

PaneCalibrationConfig Pane_Calibration_Dlg::read_config() const
{
    PaneCalibrationConfig config = default_pane_calibration_config(m_tool);
    config.design = m_design->GetSelection() == 0 ? PaneCalibrationDesign::Linear :
                    m_design->GetSelection() == 1 ? PaneCalibrationDesign::Grid : PaneCalibrationDesign::Taguchi;
    config.taguchi_levels = unsigned(m_taguchi_levels->GetValue());
    config.factors.clear();
    for (const FactorControls &controls : m_factors)
        config.factors.push_back({controls.factor, controls.enabled->GetValue(), controls.minimum->GetValue(),
                                  controls.maximum->GetValue(), unsigned(controls.levels->GetValue())});
    config.pane_width = m_pane_width->GetValue();
    config.pane_depth = m_pane_depth->GetValue();
    config.pane_height = m_pane_height->GetValue();
    config.pane_gap = m_pane_gap->GetValue();
    config.mouse_ears = m_mouse_ears->GetValue();
    config.labels = m_labels->GetValue();
    config.label_glyph_height = m_glyph_height->GetValue();
    config.label_relief = m_label_relief->GetValue();
    config.pane_extruder = m_pane_extruder->GetValue();
    config.label_extruder = m_label_extruder->GetValue();
    return config;
}

void Pane_Calibration_Dlg::refresh_preview()
{
    m_taguchi_levels->Enable(m_design->GetSelection() == 2);
    const bool common_levels = m_design->GetSelection() == 2;
    for (FactorControls &factor : m_factors)
        factor.levels->Enable(!common_levels);
    m_glyph_height->Enable(m_labels->GetValue());
    m_label_relief->Enable(m_labels->GetValue());
    m_label_extruder->Enable(m_labels->GetValue());
    try {
        const PaneCalibrationPlan plan = build_pane_calibration_plan(read_config());
        m_preview->SetLabel(wxString::Format(_L("%s · %zu panes · row-major sequential order"),
                                             from_u8(plan.array_name), plan.rows.size()));
        m_preview->SetForegroundColour(GetForegroundColour());
        m_generate->Enable(true);
    } catch (const std::exception &error) {
        m_preview->SetLabel(from_u8(error.what()));
        m_preview->SetForegroundColour(wxColour(190, 45, 45));
        m_generate->Enable(false);
    }
    Layout();
}

void Pane_Calibration_Dlg::on_start(wxCommandEvent &)
{
    const PaneCalibrationConfig config = read_config();
    const std::string section = m_tool == PaneCalibrationTool::ClearFilament ? "clear_filament_calibration" : "ironing_calibration";
    wxGetApp().app_config->set(section, "design", std::to_string(m_design->GetSelection()));
    wxGetApp().app_config->set(section, "taguchi_levels", std::to_string(config.taguchi_levels));
    for (const PaneCalibrationFactorSetting &factor : config.factors) {
        const std::string prefix = pane_calibration_factor_key(factor.factor) + "_";
        wxGetApp().app_config->set(section, prefix + "enabled", factor.enabled ? "1" : "0");
        wxGetApp().app_config->set(section, prefix + "minimum", std::to_string(factor.minimum));
        wxGetApp().app_config->set(section, prefix + "maximum", std::to_string(factor.maximum));
        wxGetApp().app_config->set(section, prefix + "levels", std::to_string(factor.levels));
    }
    wxGetApp().app_config->set(section, "pane_width", std::to_string(config.pane_width));
    wxGetApp().app_config->set(section, "pane_depth", std::to_string(config.pane_depth));
    wxGetApp().app_config->set(section, "pane_height", std::to_string(config.pane_height));
    wxGetApp().app_config->set(section, "pane_gap", std::to_string(config.pane_gap));
    wxGetApp().app_config->set(section, "mouse_ears", config.mouse_ears ? "1" : "0");
    wxGetApp().app_config->set(section, "labels", config.labels ? "1" : "0");
    wxGetApp().app_config->set(section, "glyph_height", std::to_string(config.label_glyph_height));
    wxGetApp().app_config->set(section, "label_relief", std::to_string(config.label_relief));
    wxGetApp().app_config->set(section, "pane_extruder", std::to_string(config.pane_extruder));
    wxGetApp().app_config->set(section, "label_extruder", std::to_string(config.label_extruder));
    const DynamicPrintConfig full_config = wxGetApp().preset_bundle->full_config();
    const auto *temperature_low = full_config.opt<ConfigOptionInts>("nozzle_temperature_range_low");
    const auto *temperature_high = full_config.opt<ConfigOptionInts>("nozzle_temperature_range_high");
    for (const PaneCalibrationFactorSetting &factor : config.factors) {
        if (!factor.enabled || factor.factor != PaneCalibrationFactor::NozzleTemperature ||
            temperature_low == nullptr || temperature_high == nullptr)
            continue;
        const size_t extruder = size_t(std::max(config.pane_extruder, 1) - 1);
        const int low = temperature_low->get_at(extruder);
        const int high = temperature_high->get_at(extruder);
        if (factor.minimum < low || factor.maximum > high) {
            MessageDialog confirm(this,
                wxString::Format(_L("The requested temperature range %.0f–%.0f °C is outside the active filament range %d–%d °C. Continue?"),
                                 factor.minimum, factor.maximum, low, high),
                _L("Temperature safety warning"), wxICON_WARNING | wxYES_NO);
            if (confirm.ShowModal() != wxID_YES)
                return;
        }
    }
    m_plater->calib_panes(config);
    EndModal(wxID_OK);
}

void Pane_Calibration_Dlg::on_dpi_changed(const wxRect &)
{
    Refresh();
    Layout();
}

}} // namespace Slic3r::GUI
