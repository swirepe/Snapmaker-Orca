#ifndef slic3r_calib_dlg_hpp_
#define slic3r_calib_dlg_hpp_

#include "wxExtensions.hpp"
#include "GUI_Utils.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/RoundedRectangle.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/CheckBox.hpp"
#include "Widgets/ComboBox.hpp"
#include "Widgets/TextInput.hpp"
#include "Widgets/LabeledStaticBox.hpp"
#include "Widgets/RadioGroup.hpp"
#include "GUI_App.hpp"
#include <wx/checkbox.h>
#include <wx/choice.h>
#include "wx/hyperlink.h"
#include <wx/choice.h>
#include <wx/radiobox.h>
#include <wx/scrolwin.h>
#include <wx/spinctrl.h>
#include <wx/textctrl.h>
#include "libslic3r/calib.hpp"
#include "libslic3r/FuzzySkinCalibration.hpp"

namespace Slic3r { namespace GUI {

class PA_Calibration_Dlg : public DPIDialog
{
public:
    PA_Calibration_Dlg(wxWindow* parent, wxWindowID id, Plater* plater);
    ~PA_Calibration_Dlg();
    void on_dpi_changed(const wxRect& suggested_rect) override;
	void on_show(wxShowEvent& event);
protected:
    void reset_params();
	virtual void on_start(wxCommandEvent& event);
	virtual void on_extruder_type_changed(wxCommandEvent& event);
	virtual void on_method_changed(wxCommandEvent& event);

protected:
	bool m_bDDE;
	Calib_Params m_params;


	RadioGroup* m_rbExtruderType;
	RadioGroup* m_rbMethod;
	TextInput* m_tiStartPA;
	TextInput* m_tiEndPA;
	TextInput* m_tiPAStep;
	::CheckBox* m_cbPrintNum;
	TextInput* m_tiBMAccels;
	TextInput* m_tiBMSpeeds;

	Plater* m_plater;
};

class Temp_Calibration_Dlg : public DPIDialog
{
public:
    Temp_Calibration_Dlg(wxWindow* parent, wxWindowID id, Plater* plater);
    ~Temp_Calibration_Dlg();
    void on_dpi_changed(const wxRect& suggested_rect) override;

protected:
    
    virtual void on_start(wxCommandEvent& event);
    virtual void on_filament_type_changed(wxCommandEvent& event);
    Calib_Params m_params;

    RadioGroup* m_rbFilamentType;
    TextInput* m_tiStart;
    TextInput* m_tiEnd;
    TextInput* m_tiStep;
    Plater* m_plater;
};

class Thermal_Pattern_Calibration_Dlg : public DPIDialog
{
public:
    Thermal_Pattern_Calibration_Dlg(wxWindow *parent, wxWindowID id, Plater *plater);
    void on_dpi_changed(const wxRect &suggested_rect) override;

protected:
    void on_generate(wxCommandEvent &event);
    void on_apply(wxCommandEvent &event);
    bool read_params(Calib_Params &params, bool warn_about_filament_limit);
    size_t selected_tool() const;
    void   load_selected_tool_defaults();

    wxChoice*  m_choice_tool;
    TextInput *m_ti_base;
    TextInput *m_ti_step;
    TextInput *m_ti_levels;
    TextInput *m_ti_band_height;
    Plater   *m_plater;
};

class MaxVolumetricSpeed_Test_Dlg : public DPIDialog
{
public:
    MaxVolumetricSpeed_Test_Dlg(wxWindow* parent, wxWindowID id, Plater* plater);
    ~MaxVolumetricSpeed_Test_Dlg();
    void on_dpi_changed(const wxRect& suggested_rect) override;

protected:

    virtual void on_start(wxCommandEvent& event);
    Calib_Params m_params;

    TextInput* m_tiStart;
    TextInput* m_tiEnd;
    TextInput* m_tiStep;
    Plater* m_plater;
};

class VFA_Test_Dlg : public DPIDialog {
public:
    VFA_Test_Dlg(wxWindow* parent, wxWindowID id, Plater* plater);
    ~VFA_Test_Dlg();
    void on_dpi_changed(const wxRect& suggested_rect) override;

protected:
    virtual void on_start(wxCommandEvent& event);
    Calib_Params m_params;

    TextInput* m_tiStart;
    TextInput* m_tiEnd;
    TextInput* m_tiStep;
    Plater* m_plater;
};


class Retraction_Test_Dlg : public DPIDialog
{
public:
    Retraction_Test_Dlg (wxWindow* parent, wxWindowID id, Plater* plater);
    ~Retraction_Test_Dlg ();
    void on_dpi_changed(const wxRect& suggested_rect) override;

protected:

    virtual void on_start(wxCommandEvent& event);
    Calib_Params m_params;

    TextInput* m_tiStart;
    TextInput* m_tiEnd;
    TextInput* m_tiStep;
    Plater* m_plater;
};

class Input_Shaping_Freq_Test_Dlg : public DPIDialog
{
public:
    Input_Shaping_Freq_Test_Dlg (wxWindow* parent, wxWindowID id, Plater* plater);
    ~Input_Shaping_Freq_Test_Dlg ();
    void on_dpi_changed(const wxRect& suggested_rect) override;
    
protected:

    virtual void on_start(wxCommandEvent& event);
    Calib_Params m_params;

    RadioGroup* m_rbModel;
    TextInput* m_tiFreqStartX;
    TextInput* m_tiFreqEndX;
    TextInput* m_tiFreqStartY;
    TextInput* m_tiFreqEndY;
    TextInput* m_tiDampingFactor;
    Plater* m_plater;
};

class Input_Shaping_Damp_Test_Dlg : public DPIDialog
{
public:
    Input_Shaping_Damp_Test_Dlg (wxWindow* parent, wxWindowID id, Plater* plater);
    ~Input_Shaping_Damp_Test_Dlg ();
    void on_dpi_changed(const wxRect& suggested_rect) override;
    
protected:

    virtual void on_start(wxCommandEvent& event);
    Calib_Params m_params;

    RadioGroup* m_rbModel;
    TextInput* m_tiFreqX;
    TextInput* m_tiFreqY;
    TextInput* m_tiDampingFactorStart;
    TextInput* m_tiDampingFactorEnd;
    Plater* m_plater;
};

class Junction_Deviation_Test_Dlg : public DPIDialog
{
public:
    Junction_Deviation_Test_Dlg(wxWindow* parent, wxWindowID id, Plater* plater);
    ~Junction_Deviation_Test_Dlg();
    void on_dpi_changed(const wxRect& suggested_rect) override;
    
protected:
    virtual void on_start(wxCommandEvent& event);
    Calib_Params m_params;

    RadioGroup* m_rbModel;
    TextInput* m_tiJDStart;
    TextInput* m_tiJDEnd;
    Plater* m_plater;
};

class Pane_Calibration_Dlg : public DPIDialog
{
public:
    Pane_Calibration_Dlg(wxWindow *parent, wxWindowID id, Plater *plater, PaneCalibrationTool tool);
    void on_dpi_changed(const wxRect &suggested_rect) override;

private:
    struct FactorControls {
        PaneCalibrationFactor factor;
        wxCheckBox            *enabled {nullptr};
        wxSpinCtrlDouble      *minimum {nullptr};
        wxSpinCtrlDouble      *maximum {nullptr};
        wxSpinCtrl            *levels {nullptr};
    };

    PaneCalibrationConfig read_config() const;
    void                  refresh_printer_capabilities();
    void                  refresh_preview();
    void                  on_start(wxCommandEvent &event);
    void                  on_show(wxShowEvent &event);

    Plater                      *m_plater {nullptr};
    PaneCalibrationTool          m_tool {PaneCalibrationTool::ClearFilament};
    wxChoice                    *m_design {nullptr};
    wxSpinCtrl                  *m_taguchi_levels {nullptr};
    std::vector<FactorControls>  m_factors;
    wxSpinCtrlDouble            *m_pane_width {nullptr};
    wxSpinCtrlDouble            *m_pane_depth {nullptr};
    wxSpinCtrlDouble            *m_pane_height {nullptr};
    wxSpinCtrlDouble            *m_pane_gap {nullptr};
    wxCheckBox                  *m_mouse_ears {nullptr};
    wxCheckBox                  *m_labels {nullptr};
    wxSpinCtrlDouble            *m_glyph_height {nullptr};
    wxSpinCtrlDouble            *m_label_relief {nullptr};
    wxSpinCtrl                  *m_pane_extruder {nullptr};
    wxSpinCtrl                  *m_label_extruder {nullptr};
    wxStaticText                *m_preview {nullptr};
    Button                      *m_generate {nullptr};
};

class Fuzzy_Skin_Calibration_Dlg : public DPIDialog
{
public:
    Fuzzy_Skin_Calibration_Dlg(wxWindow* parent, wxWindowID id, Plater* plater);
    void on_dpi_changed(const wxRect& suggested_rect) override;

private:
    FuzzySkinCalibrationConfig read_config() const;
    void                       on_start(wxCommandEvent& event);
    void                       on_mode_changed(wxCommandEvent& event);

    Plater*     m_plater{nullptr};
    wxChoice*   m_mode{nullptr};
    wxChoice*   m_layout{nullptr};
    wxTextCtrl* m_thickness_min{nullptr};
    wxTextCtrl* m_thickness_max{nullptr};
    wxTextCtrl* m_thickness_step{nullptr};
    wxTextCtrl* m_distance_min{nullptr};
    wxTextCtrl* m_distance_max{nullptr};
    wxTextCtrl* m_distance_step{nullptr};
    wxTextCtrl* m_coupon_size{nullptr};
    wxCheckBox* m_labels{nullptr};
};
}} // namespace Slic3r::GUI
#endif
