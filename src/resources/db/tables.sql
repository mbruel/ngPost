-- Replacement of csv file POST_HISTORY
-- date;nzb name;size;avg. speed;archive name;archive pass;groups;from
-- used to know unfinished post and allow to display global stats

CREATE TABLE IF NOT EXISTS tHistory (
        id           INTEGER PRIMARY KEY AUTOINCREMENT,
        date         TEXT NOT NULL,
        nzbName      TEXT NOT NULL,
        size         TEXT NOT NULL,
        avgSpeed     TEXT NOT NULL,
        archiveName  TEXT NOT NULL,
        archivePass  TEXT NOT NULL,
        groups       TEXT NOT NULL,
        poster       TEXT NOT NULL,
        tmpPath      TEXT NOT NULL,
        nzbFilePath  TEXT NOT NULL,
        done         INTEGER DEFAULT 0
);

CREATE INDEX index_done ON tHistory(done);
