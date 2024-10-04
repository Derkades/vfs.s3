#include <kodi/addon-instance/VFS.h>
#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/ListBucketsRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/GetObjectAttributesRequest.h>
#include <aws/s3/model/ListObjectsV2Request.h>

class S3Context {
    public:
        std::string bucketName;
        std::string objectName;
        int64_t position;
        int64_t size;
        Aws::S3::S3Client *client;
        S3Context(std::string bucketName, std::string objectName, int64_t size, Aws::S3::S3Client *client)
        : bucketName{bucketName}, objectName{objectName}, size{size}, client{client} {
            this->position = 0;
        };
};

class ATTR_DLL_LOCAL CS3VFS : public kodi::addon::CInstanceVFS {
    private:
        Aws::S3::S3Client *GetClient(const kodi::addon::VFSUrl& url) {
            kodi::Log(ADDON_LOG_INFO, "GetClient");
            kodi::Log(ADDON_LOG_INFO, "URL: %s", url.GetURL().c_str());
            kodi::Log(ADDON_LOG_INFO, "filename: %s", url.GetFilename().c_str());
            kodi::Log(ADDON_LOG_INFO, "hostname: %s", url.GetHostname().c_str());
            kodi::Log(ADDON_LOG_INFO, "username: %s", url.GetUsername().c_str());
            Aws::S3::S3ClientConfiguration clientConfig;
            clientConfig.allowSystemProxy = true;
            clientConfig.endpointOverride = std::string("http://") + url.GetHostname() + std::to_string(url.GetPort());
            clientConfig.region = std::string("garage");
            kodi::Log(ADDON_LOG_INFO, "new credentials");
            Aws::Auth::AWSCredentials credentials;
            credentials.SetAWSAccessKeyId(url.GetUsername());
            credentials.SetAWSSecretKey(url.GetPassword());
            kodi::Log(ADDON_LOG_INFO, "new S3Client");
            return new Aws::S3::S3Client(credentials, nullptr, clientConfig);
        }

    public:
        CS3VFS(const kodi::addon::IInstanceInfo& instance) : CInstanceVFS(instance) {}

        kodi::addon::VFSFileHandle Open(const kodi::addon::VFSUrl& url) override {
            std::size_t firstslash = url.GetFilename().find('/');
            if (firstslash == std::string::npos) {
                // First directory must be bucket, can't open root
                return nullptr;
            }

            std::string bucketName = url.GetFilename().substr(0, firstslash);
            std::string objectName = url.GetFilename().substr(firstslash + 1);

            Aws::S3::S3Client *client = this->GetClient(url);

            Aws::S3::Model::GetObjectAttributesRequest request;
            request.SetBucket(bucketName);
            request.SetKey(objectName);
            Aws::S3::Model::GetObjectAttributesOutcome outcome = client->GetObjectAttributes(request);
            if (!outcome.IsSuccess()) {
                const Aws::S3::S3Error &err = outcome.GetError();
                kodi::Log(ADDON_LOG_INFO, "getObjectAttributes %s: %s", err.GetExceptionName().c_str(), err.GetMessage().c_str());
                delete client;
                return nullptr;
            }
            Aws::S3::Model::GetObjectAttributesResult &result = outcome.GetResult();
            int64_t size = result.GetObjectSize();

            S3Context *context = new S3Context(bucketName, objectName, size, client);

            return context;
        }

        bool Close(kodi::addon::VFSFileHandle vfsContext) override {
            S3Context *context = (S3Context*) vfsContext;
            delete context->client;
            delete context;
            return true;
        }

        ssize_t Read(kodi::addon::VFSFileHandle vfsContext, uint8_t* buffer, size_t uiBufSize) override {
            S3Context *context = (S3Context*) vfsContext;

            kodi::Log(ADDON_LOG_INFO, "call to Read");

            Aws::S3::Model::GetObjectRequest request;
            request.SetBucket(context->bucketName);
            request.SetKey(context->objectName);
            request.SetRange("bytes=" + std::to_string(context->position) + "-" + std::to_string(context->position + uiBufSize));

            Aws::S3::Model::GetObjectOutcome outcome = context->client->GetObject(request);

            if (!outcome.IsSuccess()) {
                const Aws::S3::S3Error &err = outcome.GetError();
                kodi::Log(ADDON_LOG_ERROR, "getObject error: %s: %s", err.GetExceptionName().c_str(), err.GetMessage().c_str());
                return -1;
            }

            Aws::S3::Model::GetObjectResult &result = outcome.GetResult();
            result.GetBody() >> std::setw(uiBufSize) >> buffer;
            context->position = result.GetContentLength();
            return result.GetContentLength();
        }

        int64_t Seek(kodi::addon::VFSFileHandle vfsContext, int64_t position, int whence) override {
            S3Context *context = (S3Context*) vfsContext;
            switch(whence) {
                case SEEK_SET:
                    context->position = position;
                    return position;
                case SEEK_CUR:
                    context->position += position;
                    break;
                case SEEK_END:
                    context->position += context->size + position;
                    break;
            }
            if (context->position < 0) {
                kodi::Log(ADDON_LOG_ERROR, "illegal seek 1");
                context->position = 0;
            } else if (context->position > context->size) {
                kodi::Log(ADDON_LOG_ERROR, "illegal seek 2");
                context->position = context->size;
            }
            return context->position;
        }

        int64_t GetLength(kodi::addon::VFSFileHandle vfsContext) override {
            S3Context *context = (S3Context*) vfsContext;
            return context->size;
        }

        int64_t GetPosition(kodi::addon::VFSFileHandle vfsContext) override {
            S3Context *context = (S3Context*) vfsContext;
            return context->size;
        }

        int GetChunkSize(kodi::addon::VFSFileHandle context) override {
            return 1024*1024;
        }

        bool IoControlGetSeekPossible(kodi::addon::VFSFileHandle context) override {
            return true;
        }

        bool IoControlGetCacheStatus(kodi::addon::VFSFileHandle context,
                                        kodi::vfs::CacheStatus& status) override {
            return false;
        }

        bool IoControlSetCacheRate(kodi::addon::VFSFileHandle context, uint32_t rate) override{
            return false;
        }

        bool IoControlSetRetry(kodi::addon::VFSFileHandle context, bool retry) override {
            return false;
        }

        int Stat(const kodi::addon::VFSUrl& url, kodi::vfs::FileStatus& buffer) override {
            return -1;
        }

        bool Exists(const kodi::addon::VFSUrl& url) override {
            kodi::Log(ADDON_LOG_INFO, "call to Exists with filename: %s", url.GetFilename().c_str());
            // TODO
            return false;
        }

        bool DirectoryExists(const kodi::addon::VFSUrl& url) override {
            kodi::Log(ADDON_LOG_INFO, "call to DirectoryExists with filename: %s", url.GetFilename().c_str());
            // TODO
            return false;
        }

        bool GetDirectory(const kodi::addon::VFSUrl& url,
                          std::vector<kodi::vfs::CDirEntry>& items,
                          CVFSCallbacks callbacks) override {
            kodi::Log(ADDON_LOG_INFO, "call to GetDirectory");

            Aws::S3::S3Client *client = this->GetClient(url);

            std::size_t firstslash = url.GetFilename().find('/');
            if (firstslash == std::string::npos) {
                // No first slash, must list buckets instead of directories

                kodi::Log(ADDON_LOG_INFO, "ListBuckets");

                Aws::S3::Model::ListBucketsRequest brequest;
                Aws::S3::Model::ListBucketsOutcome boutcome = client->ListBuckets(brequest);

                if (!boutcome.IsSuccess()) {
                    kodi::Log(ADDON_LOG_ERROR, "Error ListBuckets: %s", boutcome.GetError().GetMessage().c_str());
                    delete client;
                    return false;
                }

                Aws::S3::Model::ListBucketsResult &bresult = boutcome.GetResult();

                for (Aws::S3::Model::Bucket bucket : bresult.GetBuckets()) {
                    kodi::vfs::CDirEntry kentry;
                    kentry.SetFolder(true);
                    kentry.SetLabel(bucket.GetName());
                    kentry.SetPath(bucket.GetName());
                    items.push_back(kentry);
                }

                delete client;
                return true;
            }

            kodi::Log(ADDON_LOG_INFO, "ListObjects");

            std::string bucketName = url.GetFilename().substr(0, firstslash);
            std::string objectName = url.GetFilename().substr(firstslash + 1);

            kodi::Log(ADDON_LOG_INFO, "bucketName: %s objectName: %s", bucketName.c_str(), objectName.c_str());

            Aws::S3::Model::ListObjectsV2Request orequest;
            orequest.WithBucket(bucketName);

            Aws::String continuationToken; // Used for pagination.
            Aws::Vector<Aws::S3::Model::Object> allObjects;

            do {
                if (!continuationToken.empty()) {
                    orequest.SetContinuationToken(continuationToken);
                }

                Aws::S3::Model::ListObjectsV2Outcome ooutcome = client->ListObjectsV2(orequest);

                if (!ooutcome.IsSuccess()) {
                    std::cerr << "Error: listObjects: " << ooutcome.GetError().GetMessage() << std::endl;
                    delete client;
                    return false;
                } else {
                    for (Aws::S3::Model::Object object : ooutcome.GetResult().GetContents()) {
                        std::cerr << "listObject: " << object.GetKey() << std::endl;

                        // kodi::vfs::CDirEntry kentry;
                        // kentry.SetLabel(bucket.GetName());
                        // kentry.SetPath(bucket.GetName());
                        // items.push_back(kentry);
                    }

                    continuationToken = ooutcome.GetResult().GetNextContinuationToken();
                }
            } while (!continuationToken.empty());

            delete client;
            return true;
        }
};

class ATTR_DLL_LOCAL CS3Addon : public kodi::addon::CAddonBase {
    public:
        CS3Addon() = default;

        ADDON_STATUS CreateInstance(const kodi::addon::IInstanceInfo& instance,
                                    KODI_ADDON_INSTANCE_HDL& hdl) override {
            kodi::Log(ADDON_LOG_INFO, "Creating my VFS instance");
            hdl = new CS3VFS(instance);
            return ADDON_STATUS_OK;
        }
};

ADDONCREATOR(CS3Addon)
