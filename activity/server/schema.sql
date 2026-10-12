CREATE TABLE IF NOT EXISTS users (
 id uuid PRIMARY KEY, name text NOT NULL, email text NOT NULL UNIQUE, password text NOT NULL,
 role text NOT NULL CHECK(role IN ('admin','mapper')), enabled boolean NOT NULL DEFAULT true
);
CREATE TABLE IF NOT EXISTS logins (token text PRIMARY KEY, user_id uuid NOT NULL REFERENCES users(id), expires_at timestamptz NOT NULL);
CREATE TABLE IF NOT EXISTS projects (id uuid PRIMARY KEY, name text NOT NULL);
CREATE TABLE IF NOT EXISTS members (project_id uuid REFERENCES projects(id), user_id uuid REFERENCES users(id), PRIMARY KEY(project_id,user_id));
CREATE TABLE IF NOT EXISTS maps (id uuid PRIMARY KEY, project_id uuid NOT NULL REFERENCES projects(id), name text NOT NULL);
ALTER TABLE maps ADD COLUMN IF NOT EXISTS filename text NOT NULL DEFAULT '';
ALTER TABLE maps ADD COLUMN IF NOT EXISTS tracked boolean NOT NULL DEFAULT false;
-- Migrate existing maps already named after an OTBM file. No local paths needed.
UPDATE maps SET filename=name,tracked=true WHERE filename='' AND name ~* '^[^/\\]+\.otbm$';
CREATE TABLE IF NOT EXISTS pairing_codes (hash text PRIMARY KEY, user_id uuid NOT NULL REFERENCES users(id), expires_at timestamptz NOT NULL);
CREATE TABLE IF NOT EXISTS devices (
 id uuid PRIMARY KEY, user_id uuid NOT NULL REFERENCES users(id), token text NOT NULL UNIQUE,
 name text NOT NULL, revoked boolean NOT NULL DEFAULT false, last_received timestamptz, last_event timestamptz,
 status text NOT NULL DEFAULT 'offline', current_map uuid REFERENCES maps(id)
);
CREATE TABLE IF NOT EXISTS sessions (
 id uuid PRIMARY KEY, user_id uuid NOT NULL REFERENCES users(id), device_id uuid NOT NULL REFERENCES devices(id),
 map_id uuid NOT NULL REFERENCES maps(id), note text NOT NULL DEFAULT ''
);
CREATE TABLE IF NOT EXISTS events (
 id uuid PRIMARY KEY, device_id uuid NOT NULL REFERENCES devices(id), session_id uuid REFERENCES sessions(id),
 seq bigint NOT NULL CHECK(seq >= 0), at timestamptz NOT NULL, kind text NOT NULL, payload jsonb NOT NULL,
 UNIQUE(device_id,session_id,seq)
);
CREATE INDEX IF NOT EXISTS events_at ON events(at);
CREATE INDEX IF NOT EXISTS events_session ON events(session_id,at);

CREATE INDEX IF NOT EXISTS sessions_user ON sessions(user_id);
CREATE INDEX IF NOT EXISTS sessions_map ON sessions(map_id);
