<?php
// Loopback-only relay to the Node API. Does not execute paths or shell commands.
if (!in_array($_SERVER['REMOTE_ADDR'] ?? '', ['127.0.0.1', '::1'], true)) {
    http_response_code(403); exit('Local pilot only');
}
$path = $_GET['path'] ?? '';
if (!is_string($path) || strlen($path) > 2048 || !preg_match('~^/(login|logout|me|catalog|users|projects|maps|members|pairing|report|devices|sessions)(/|\?|$)~', $path) || preg_match('/[\r\n]/', $path)) {
    http_response_code(400); exit('Invalid API path');
}
$method = $_SERVER['REQUEST_METHOD'];
if (!in_array($method, ['GET', 'POST', 'PUT', 'PATCH', 'DELETE'], true)) {
    http_response_code(405); exit;
}
$body = file_get_contents('php://input', false, null, 0, 1048577);
if (strlen($body) > 1048576) { http_response_code(413); exit; }
$headers = "Content-Type: application/json\r\n";
if (isset($_COOKIE['dme_session']) && preg_match('/^[a-f0-9]{64}$/', $_COOKIE['dme_session'])) $headers .= 'Cookie: dme_session=' . $_COOKIE['dme_session'] . "\r\n";
if (isset($_SERVER['HTTP_ORIGIN'])) $headers .= 'Origin: ' . str_replace(["\r", "\n"], '', $_SERVER['HTTP_ORIGIN']) . "\r\n";
$context = stream_context_create(['http' => ['method'=>$method, 'header'=>$headers, 'content'=>$body, 'ignore_errors'=>true, 'timeout'=>20, 'follow_location'=>0]]);
$response = @file_get_contents('http://127.0.0.1:3000/api' . $path, false, $context);
if ($response === false) { http_response_code(502); header('Content-Type: application/json'); echo '{"error":"DME Activity API is not running"}'; exit; }
foreach ($http_response_header as $header) {
    if (preg_match('~^HTTP/\S+ (\d+)~', $header, $match)) http_response_code((int)$match[1]);
    elseif (stripos($header, 'Set-Cookie:') === 0 || stripos($header, 'Content-Type:') === 0) header($header, false);
}
header('Cache-Control: no-store');
echo $response;
