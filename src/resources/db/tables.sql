-- Replacement of csv file POST_HISTORY
-- date;nzb name;size;avg. speed;archive name;archive pass;groups;from
-- used to know unfinished post and allow to display global stats
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS tHistory (
        id           INTEGER PRIMARY KEY AUTOINCREMENT,
        date         TEXT NOT NULL,
        nzbName      TEXT NOT NULL,
        sizeMB       REAL NOT NULL,
        avgSpeedKbps REAL NOT NULL,
        archiveName  TEXT,
        archivePass  TEXT,
        groups       TEXT NOT NULL,
        poster       TEXT NOT NULL,
        packingPath  TEXT,
        nzbFilePath  TEXT NOT NULL,
        nbFiles      INTEGER NOT NULL,
        state        INTEGER DEFAULT 0
);
CREATE INDEX index_state ON tHistory(state);

CREATE TABLE IF NOT EXISTS tUnfinishedFiles (
        job_id           INTEGER NOT NULL,
        filePath         TEXT NOT NULL,
        FOREIGN KEY (job_id) REFERENCES tHistory(id)
);

