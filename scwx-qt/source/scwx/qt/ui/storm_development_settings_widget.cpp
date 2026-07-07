#include <scwx/qt/ui/storm_development_settings_widget.hpp>
#include <scwx/qt/settings/storm_development_settings.hpp>

#include <QCheckBox>
#include <QFormLayout>
#include <QSpinBox>

namespace scwx::qt::ui
{

class StormDevelopmentSettingsWidget::Impl
{
public:
   explicit Impl() = default;
   ~Impl()         = default;

   QCheckBox* enabledCheckBox_ {nullptr};
   QSpinBox*  refreshIntervalSpinBox_ {nullptr};
};

StormDevelopmentSettingsWidget::StormDevelopmentSettingsWidget(
   QWidget* parent) :
    QWidget(parent), p(std::make_unique<Impl>())
{
   auto& settings = settings::StormDevelopmentSettings::Instance();

   auto* layout = new QFormLayout(this);

   p->enabledCheckBox_ = new QCheckBox(tr("Enabled"), this);
   p->enabledCheckBox_->setChecked(settings.enabled().GetValue());
   layout->addRow(p->enabledCheckBox_);

   p->refreshIntervalSpinBox_ = new QSpinBox(this);
   p->refreshIntervalSpinBox_->setRange(
      static_cast<int>(settings.refresh_interval_minutes().GetMinimum().value()),
      static_cast<int>(settings.refresh_interval_minutes().GetMaximum().value()));
   p->refreshIntervalSpinBox_->setValue(
      static_cast<int>(settings.refresh_interval_minutes().GetValue()));
   p->refreshIntervalSpinBox_->setSuffix(tr(" min"));
   layout->addRow(tr("Refresh interval"), p->refreshIntervalSpinBox_);

   connect(p->enabledCheckBox_,
           &QCheckBox::toggled,
           this,
           [](bool checked)
           {
              settings::StormDevelopmentSettings::Instance().enabled().SetValue(
                 checked);
           });

   connect(p->refreshIntervalSpinBox_,
           QOverload<int>::of(&QSpinBox::valueChanged),
           this,
           [](int value)
           {
              settings::StormDevelopmentSettings::Instance()
                 .refresh_interval_minutes()
                 .SetValue(value);
           });
}

StormDevelopmentSettingsWidget::~StormDevelopmentSettingsWidget() = default;

} // namespace scwx::qt::ui
