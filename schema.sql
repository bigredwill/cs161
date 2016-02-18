
CREATE TABLE Video_Meta(
	video_id bigserial primary key not null,
	num_frames int not null,
	x_resolution int not null,
	y_resolution int not null,
	fps real
);

CREATE TYPE bbox_type AS ENUM (
	'Face',
	'LEye',
	'REye',
	'Nose',
	'Mouth'
);

CREATE TABLE Bounding_Boxes (
	bbox_id bigserial not null,
	video_id bigint not null,
	frame_number bigint not null,
	bbox_type bbox_type,
	upper_left_corner_coord point not null,
	width int not null,
	height int not null
);

CREATE TABLE Pupils (
	video_id bigint not null,
	frame_number bigint not null,
	bbox_id bigint not null,
	left_eye_coords point not null,
	right_eye_coords point not null
);

CREATE TABLE STASM (
	video_id bigint not null,
	frame_number bigint not null,
	stasm_coords point[]
);