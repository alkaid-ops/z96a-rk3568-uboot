#!/usr/bin/env python3
import os
import sys
import time
import requests

BASE = os.environ.get("HA_URL", "http://192.168.31.18:8123")
ENTITY = os.environ.get("HA_ENTITY", "switch.giot_cn_1128443935_v6shsm_on_p_2_1")
USER = os.environ.get("HA_USER", "hsy")
PASSWORD = os.environ.get("HA_PASSWORD", "Hsy8862669.")


def login():
    client_id = BASE + "/"
    s = requests.Session()
    r = s.post(
        BASE + "/auth/login_flow",
        json={
            "client_id": client_id,
            "handler": ["homeassistant", None],
            "redirect_uri": client_id + "?auth_callback=1",
        },
        timeout=10,
    )
    r.raise_for_status()
    flow_id = r.json()["flow_id"]
    r = s.post(
        BASE + f"/auth/login_flow/{flow_id}",
        json={"client_id": client_id, "username": USER, "password": PASSWORD},
        timeout=10,
    )
    r.raise_for_status()
    result = r.json()
    if result.get("type") != "create_entry":
        raise RuntimeError(f"HA login failed: {result}")
    code = result["result"]
    r = s.post(
        BASE + "/auth/token",
        data={
            "grant_type": "authorization_code",
            "code": code,
            "client_id": client_id,
        },
        timeout=10,
    )
    r.raise_for_status()
    return r.json()["access_token"]


def api(method, path, token, **kwargs):
    r = requests.request(
        method,
        BASE + path,
        headers={"Authorization": "Bearer " + token},
        timeout=15,
        **kwargs,
    )
    r.raise_for_status()
    return r.json() if r.content else None


def state(token):
    return api("GET", f"/api/states/{ENTITY}", token)


def service(token, name):
    return api(
        "POST",
        f"/api/services/switch/{name}",
        token,
        json={"entity_id": ENTITY},
    )


def main():
    command = sys.argv[1] if len(sys.argv) > 1 else "status"
    token = login()
    if command == "on":
        service(token, "turn_on")
    elif command == "off":
        service(token, "turn_off")
    elif command == "cycle":
        service(token, "turn_off")
        time.sleep(5)
        service(token, "turn_on")
    elif command != "status":
        raise SystemExit("usage: ha_switch.py on|off|status|cycle")
    st = state(token)
    print(f"{st['entity_id']} {st['state']} {st['last_changed']}")
    for key in ("power", "current", "voltage"):
        if key in st.get("attributes", {}):
            print(f"{key}={st['attributes'][key]}")


if __name__ == "__main__":
    main()
