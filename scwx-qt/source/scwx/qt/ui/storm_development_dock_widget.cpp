#include <scwx/qt/ui/storm_development_dock_widget.hpp>
#include <scwx/qt/ui/storm_development_heat_map_widget.hpp>
#include <scwx/qt/manager/storm_development_manager.hpp>

#include <QDateTime>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

namespace scwx::qt::ui
{

class StormDevelopmentDockWidget::Impl
{
public:
   explicit Impl() : manager_ {manager::StormDevelopmentManager::Instance()} {}
   ~Impl() = default;

   std::shared_ptr<manager::StormDevelopmentManager> manager_;

   QLabel*                        statusLabel_ {nullptr};
   StormDevelopmentHeatMapWidget* heatMapWidget_ {nullptr};
};

StormDevelopmentDockWidget::StormDevelopmentDockWidget(QWidget* parent) :
    QDockWidget(parent), p(std::make_unique<Impl>())
{
   setWindowTitle(tr("Storm Development"));
   setObjectName(QStringLiteral("stormDevelopmentDockWidget"));

   auto* content    = new QWidget(this);
   auto* mainLayout = new QVBoxLayout(content);

   p->statusLabel_ = new QLabel(tr("Waiting for data..."), content);
   mainLayout->addWidget(p->statusLabel_);

   p->heatMapWidget_ = new StormDevelopmentHeatMapWidget(content);
   mainLayout->addWidget(p->heatMapWidget_, 1);

   setWidget(content);

   auto updateFromManager =
      [this]()
   {
      p->statusLabel_->setText(
         tr("Last updated: %1")
            .arg(QDateTime::currentDateTime().toString(Qt::TextDate)));
      p->heatMapWidget_->SetGrid(p->manager_->Grid());
   };

   connect(p->manager_.get(),
           &manager::StormDevelopmentManager::GridUpdated,
           this,
           updateFromManager,
           Qt::QueuedConnection);

   updateFromManager();
}

StormDevelopmentDockWidget::~StormDevelopmentDockWidget() = default;

} // namespace scwx::qt::ui
