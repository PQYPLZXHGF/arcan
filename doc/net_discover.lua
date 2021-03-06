-- net_discover
-- @short: Spawn a frameserver that scans for local servers.
-- @inargs: callback, *optstring*
-- @longdescr: The networking frameserver has a simple UDP based broadcasting
-- protocol for local detection of servers (or remotely querying dictionary servers).
-- This function can be used to poll for available servers.
-- @note: possible fields for optstring:
--  pubkey=base64 encoded public key
--  privkey=base64 encoded private key (both pub and priv must be specified)
--  keys can be generated with net_open.
-- @note: if no public/private keypair is specified, a new pair will be generated.
-- @note: if the statustbl in the callback has its *kind* field set to discovered,
-- the *address* fill will give the host IP address, the *ident* field will
-- contain a short string identifying the service. If a public key was presented,
-- its base64 encoded form will be set in the *key* field.
-- @note: The discover process does not keep track of which servers it has seen
-- every new request beacon. It is up to the callback implementation to track
-- which ones are new and which ones have been seen before.
-- @note: *optstring* can be used to specify additional arguments that will
-- be added when calling the frameserver.
-- @group: network
-- @cfunction: net_discover
-- @related: net_open
-- @flags: experimental

