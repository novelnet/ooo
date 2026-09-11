-- ooo: ESP32-Status und Kommando-Queue
-- Zugriff ausschließlich über die Edge Function mit Service-Role-Key.
-- RLS ist an und hat KEINE Policies → anon/authenticated sehen nichts.

create table if not exists ooo_devices (
  name       text primary key,            -- 'esp'
  last_seen  timestamptz not null default now(),
  info       jsonb not null default '{}'::jsonb
);

create table if not exists ooo_commands (
  id         bigserial primary key,
  action     text not null,               -- 'wake' | 'power'
  payload    jsonb not null default '{}'::jsonb,
  created_at timestamptz not null default now(),
  acked_at   timestamptz,
  result     text
);

create index if not exists ooo_commands_pending_idx
  on ooo_commands (created_at) where acked_at is null;

alter table ooo_devices  enable row level security;
alter table ooo_commands enable row level security;
