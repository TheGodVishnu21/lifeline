// components/HistoryLog.jsx — persisted activity feed (GET /api/history)

export default function HistoryLog({ entries, onRefresh }) {
  return (
    <>
      <div className="divider" />
      <div style={{ display: 'flex', justifyContent: 'space-between',
                    alignItems: 'center' }}>
        <div className="section-tag" style={{ color: 'var(--muted)' }}>
          ACTIVITY LOG
        </div>
        <button className="btn-ghost" onClick={onRefresh}
                style={{ padding: '5px 10px', fontSize: 11 }}>
          refresh
        </button>
      </div>
      {entries.length > 0 && (
        <section id="historyOut" style={{ display: 'flex' }}>
          {entries.map((e, i) => (
            <div key={i} className="hist-row">
              <span className="ev">{e.event}</span>
              <span>{e.detail}</span>
              <span>{e.timestamp.split(' ')[1] || e.timestamp}</span>
            </div>
          ))}
        </section>
      )}
    </>
  );
}
