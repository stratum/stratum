# Copyright 2019-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

onos_url := http://localhost:8181/onos
onos_post := curl -sSL --user onos:rocks --noproxy localhost -X POST -H 'Content-Type:application/json'

pull:
	docker-compose pull

start:
	docker-compose up -d

stop:
	docker-compose down -t0

restart: stop start

onos-cli:
	ssh -o "UserKnownHostsFile=/dev/null" -o "StrictHostKeyChecking=no" -p 8101 onos@localhost

onos-log:
	docker-compose logs -f onos

onos-ui:
	open ${onos_url}/ui

mn-cli:
	docker-compose exec mininet screen -Urx -S cli

mn-log:
	docker-compose logs -f mininet

netcfg:
	${onos_post} ${onos_url}/v1/network/configuration/ -d@./topo/netcfg.json

reset: stop
	-rm -rf ./tmp
