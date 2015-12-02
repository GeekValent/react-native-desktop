
#include <QMetaObject>
#include <QMetaMethod>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

#include "reacttiming.h"
#include "reactbridge.h"


ReactTiming::ReactTiming(QObject* parent)
  : QObject(parent)
{
}

ReactTiming::~ReactTiming()
{
}

void ReactTiming::setBridge(ReactBridge* bridge)

{
  m_bridge = bridge;
}

ReactViewManager* ReactTiming::viewManager()
{
  return nullptr;
}

QString ReactTiming::moduleName()
{
  return "RCTTiming";
}

QStringList ReactTiming::methodsToExport()
{
  // TODO: move into moduledata
  const QMetaObject* metaObject = this->metaObject();
  const int methodCount = metaObject->methodCount();

  QStringList methods;
  for (int i = metaObject->methodOffset(); i < methodCount; ++i) {
    methods << metaObject->method(i).name();
  }
  return methods;
}

QVariantMap ReactTiming::constantsToExport()
{
  return QVariantMap{};
}

/*
 React Behaviour (objc)
 Reacts timers are based off the screen refresh callback from CADisplayLink.
 Making the resolution of the timer in line with screen refresh. (60hz?)

 It attempts to compensate for request latency, by passing the time the request
 was scheduled, synchronised clocks, oh my.

 - if the duration is 0 and it does not repeat, a callback is enqueud
   immediately, and this method returns
 - if the duration is less than 0.018secs (roughly one frame?) its coerced to 0
   (but not enqueued immediately)
 - a RCTTimer is created with the target time

 Loop:
  - the refresh cycle checks RCTTimers and triggers if they are current||past

 Notes:
  - requestAnimationFrame asks for a callback after duration 1ms, this
    effectively means it will served on the next frame refresh
  - This version; maybe the view, or some part, should be a custom node in the
    scene graph to properly sync (especially re: animations)
*/

void ReactTiming::createTimer
(
    int callbackId,
    int duration/*ms*/,
    const QDateTime& jsSchedulingTime,
    bool repeats
)
{
  qDebug() << __PRETTY_FUNCTION__ << callbackId << duration << jsSchedulingTime << repeats;
  if (duration == 0 && !repeats) {
    m_bridge->enqueueJSCall("JSTimersExecution", "callTimers", QVariantList{QVariantList{callbackId}});
    return;
  }

  QTimer *timer = new QTimer(this);
  timer->setSingleShot(!repeats);
  QObject::connect(timer, &QTimer::timeout, [=]() {
      if (m_bridge)
        m_bridge->enqueueJSCall("JSTimersExecution", "callTimers", QVariantList{QVariantList{callbackId}});
    });
  m_activeTimers[callbackId] = timer;
  if (duration < 18) {
    // XXX: mimicking RN behavior via timeouts
    timer->start((1.0 / 60) * 1000);
  } else {
    timer->start(duration);
  }
}

void ReactTiming::deleteTimer(int timerId)
{
  qDebug() << __PRETTY_FUNCTION__ << timerId;
  QMap<int, QTimer*>::iterator it = m_activeTimers.find(timerId);
  if (it == m_activeTimers.end())
    return;

  (*it)->stop();
  (*it)->deleteLater();
  m_activeTimers.erase(it);
}