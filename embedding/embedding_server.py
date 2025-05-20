from flask import Flask, request, jsonify
from sentence_transformers import SentenceTransformer
import torch

app = Flask(__name__)
model = SentenceTransformer("sentence-transformers/all-mpnet-base-v2")
if torch.cuda.is_available():
  print("USING CUDA")
  device = "cuda"
else:
  print("USING CPU")
  device = "cpu"

model.to(device)

@app.route("/embed", methods=["POST"])
def embed():
    try:
        textList = request.json["texts"]
        
        if not isinstance(textList, list):
            # Bad Request
            return jsonify({"error": "'texts' must be a list of strings"}), 400
        
        embeddings = model.encode(textList)
        
        print ("encoded data")
        
        return jsonify({
            "embedding": embeddings.tolist()
        })
    except Exception as e:
        print("Exception raised on flask server:", str(e))

        # Internal Server Error
        return jsonify({"error": str(e)}), 500

app.run(port=5000)