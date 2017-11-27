load("@com_google_protobuf//:protobuf.bzl", "cc_proto_library")
load("@grpc//bazel:grpc_build_system.bzl", "grpc_proto_library", "grpc_package")

grpc_package("proto", visibility="public")

cc_proto_library(
  name = "annotation",
  srcs = ["annotation.proto"],
  protoc = "@com_google_protobuf//:protoc",
  default_runtime = "@com_google_protobuf//:protobuf",
)

grpc_proto_library(
  name = "project_service",
  srcs = ["project_service.proto"],
  well_known_protos = False,
  use_external = True,
)
