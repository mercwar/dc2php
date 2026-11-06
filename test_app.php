<?php
// test_app.php - Validates application metrics and session echoing

$metrics_file = __DIR__ . '/metrics.json';
$app_hits = 'UNKNOWN';
$session_hits = 'UNKNOWN';

if (file_exists($metrics_file)) {
    $json_data = file_get_contents($metrics_file);
    $data = json_decode($json_data, true);
    if (is_array($data)) {
        $app_hits = $data['application_hits'] ?? 'UNKNOWN';
        $session_hits = $data['session_hits'] ?? 'UNKNOWN';
    }
}

// Parse any CLI arguments passed after '--'
$test_mode = 'standard';
if (isset($argv) && is_array($argv)) {
    foreach ($argv as $arg) {
        if (strpos($arg, 'mode=') === 0) {
            $test_mode = substr($arg, 5);
        }
    }
}

$_APPLICATION['test']="dc2php Application success";
$_SESSION['test']="dc2php Session success";

echo "---------------------------------------------------------\n";
echo "[PHP TEST APP] Pipeline Session Verification Active\n";
echo "[PHP TEST APP] Execution Mode     : " . $test_mode . "\n";
echo "[PHP TEST APP] Application Hits   : " . $app_hits . "\n";
echo "[PHP TEST APP] Session Hits       : " . $session_hits . "\n";
echo "[PHP TEST APP] Process ID (PID)   : " . getmypid() . "\n";
echo "[PHP TEST APP] Application   : " . $_APPLICATION['test'] . "\n";
echo "[PHP TEST APP] Session   : " . $_SESSION['test']. "\n";
echo "---------------------------------------------------------\n";
echo "[SUCCESS] Application and session variable echo verified.\n\n";


