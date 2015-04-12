.PHONY: dockerimage
dockerimage:
	docker build -t sm .

.PHONY: login
login: | dockerimage
	docker run -ti sm bash

	
