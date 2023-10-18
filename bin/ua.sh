MEASUREMENT_ID="G-Z94MXP18T6"

POST_DATA=$(cat <<EOF
{
  "client_id": "$UA_CLIENT_ID",
  "events": [
    {
      "name": "$UA_EVENT_NAME"
    }
  ]
}
EOF
)

# make sure this is always silent and can't cause failures
curl -X POST -H "Content-Type: application/json" -d "$POST_DATA" "https://www.google-analytics.com/mp/collect?api_secret=$UA_API_SECRET&measurement_id=$MEASUREMENT_ID" >/dev/null 2>&1 || true

