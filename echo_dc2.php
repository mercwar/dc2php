<?php
/**
 * DC2PHP Application and Session State Scope Extraction Test
 * Explicitly structures, calculates, and echoes state variable data pools.
 */

define('APP_PERSIST_FILE', 'application_store.json');
define('SESS_PERSIST_FILE', 'session_store.json');

function fetch_state($file) {
    if (!file_exists($file)) return [];
    return json_decode(file_get_contents($file), true) ?: [];
}

function commit_state($file, $data) {
    file_put_contents($file, json_encode($data, JSON_PRETTY_PRINT), LOCK_EX);
}

// =========================================================================
// 1. COMPUTE & SET VARIABLE DATA POOLS
// =========================================================================

// Application Variables (Shared Global across all thread processes)
$APPLICATION = fetch_state(APP_PERSIST_FILE);
if (empty($APPLICATION)) {
    $APPLICATION['ENVIRONMENT_NAME']  = 'DC2PHP_PRODUCTION_HOST';
    $APPLICATION['ENGINE_MODE']       = 'DETACHED_PRIVATE_SESSION_SAPI';
    $APPLICATION['GLOBAL_HIT_COUNTER'] = 0;
    $APPLICATION['SYSTEM_STABILITY']  = '100% OPERATIONAL';
}
$APPLICATION['GLOBAL_HIT_COUNTER']++;
commit_state(APP_PERSIST_FILE, $APPLICATION);

// Session Variables (Isolated User/Worker unique context data)
$SESSION = fetch_state(SESS_PERSIST_FILE);
if (empty($SESSION)) {
    $SESSION['SESSION_TOKEN']       = 'SESS_ID_' . upper(bin2hex(random_bytes(6)));
    $SESSION['SECURITY_ clearance']  = 'ROOT_DEVELOPER_EXEC';
    $SESSION['LOCAL_ITERATION_STEP'] = 0;
    $SESSION['AUTHENTICATED']       = true;
}
$SESSION['LOCAL_ITERATION_STEP']++;
commit_state(SESS_PERSIST_FILE, $SESSION);


// =========================================================================
// 2. ECHO EXTRACTED STATE VARIABLE ARRAYS
// =========================================================================

echo "=========================================================================" . PHP_EOL;
echo "             ECHOING ACTIVE APPLICATION & SESSION VARIABLE DATA          " . PHP_EOL;
echo "=========================================================================" . PHP_EOL;
printf("-> [HOST PIPELINE INTEGRATION PID]: %d\n\n", getmypid());

// Loop and Echo Application Storage Variables
echo "--- [DATA MATRIX: GLOBAL APPLICATION VARIABLES] ---" . PHP_EOL;
foreach ($APPLICATION as $key => $value) {
    if (is_bool($value)) $value = $value ? 'TRUE' : 'FALSE';
    printf("   App[\x27%s\x27] %-25s => %s\n", $key, str_repeat('.', 25 - strlen($key)), $value);
}

echo PHP_EOL;

// Loop and Echo Session Storage Variables
echo "--- [DATA MATRIX: ISOLATED SESSION VARIABLES] ---" . PHP_EOL;
foreach ($SESSION as $key => $value) {
    if (is_bool($value)) $value = $value ? 'TRUE' : 'FALSE';
    printf("   Sess[\x27%s\x27] %-24s => %s\n", $key, str_repeat('.', 24 - strlen($key)), $value);
}

echo "=========================================================================" . PHP_EOL;
echo "             STATE MATRIX POOL EXTRACTION PASS COMPLETE                  " . PHP_EOL;
echo "=========================================================================" . PHP_EOL;

// Simple lowercase helper function fallback
function upper($str) { return strtoupper($str); }
?>
